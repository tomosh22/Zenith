# GAME design — the gameplay framework primitives

This file is part of the GAME design family; the entry document is `Zenith2/Docs/Design/GAME.md`. It specifies the generic gameplay primitives of the `ZenithGameplay` module (`MODULE_NAME Gameplay`): the limits and enumerations, the registration of every component, resource, event and command kind, gameplay tags, gameplay events, state machines, timers, gameplay actions (the latent, awaitable operations), players, pawns, possession and ownership, the request queue every script and C++ request travels through, and the two per-world systems that run the rules (`Gameplay.Rules`) and emit the world-start event (`Gameplay.WorldEvents`). Abilities, attributes and effects are `Zenith2/Docs/Design/GAME/Abilities.md`; cameras are `Zenith2/Docs/Design/GAME/Cameras.md`; the Luau surface is `Zenith2/Docs/Design/GAME/Scripting.md`.

The primitives encode no game's rules (Z2-D-077 and inventory item Z2-I-GAME-10): no attribute, tag, effect, ability, timer or state name is built into the module, no damage or stat formula exists in C++, and every name is a game's data (a scene document's component value or a Luau constant). `Test_Gameplay_RegistersNoGameNames` (`Zenith2/Docs/Design/GAME/Tests.md` → "ZenithGameplay unit tests") proves the module interns no string at registration other than its own stable names, property names and configuration keys.

## Scope of this file

| Topic | Heading |
|---|---|
| limits, enumerations, the error enumeration | "Gameplay limits and enumerations" |
| every registered type and its authority | "Component and resource registration" |
| tags | "Gameplay tags" |
| events | "Gameplay events" |
| state machines | "State machines" |
| timers | "Timers" |
| latent actions | "Gameplay actions" |
| players, pawns, possession, ownership | "Players and possession" |
| the request path | "The request queue" |
| the rules system and its row-ownership rule | "The rules system", "Row ownership in the rules node" |
| the world-start event | "The world events system" |
| memory | "Gameplay memory accounting" |
| contract blocks of the record, component and event types | "Value type contract blocks" |

## Gameplay limits and enumerations

```cpp
// Zenith2/Engine/ZenithGameplay/Zenith_GameplayTypes.h
#pragma once
#include "ZenithCore/Zenith_Types.h"

/// @contract functions GameplayLimits
/// Ownership: Compile-time constants; no storage.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: Counts; ticks for uGAMEPLAY_ACTION_RETENTION_TICKS; bytes for uGAMEPLAY_MAX_REQUEST_PAYLOAD_BYTES.
/// Errors: None; every capacity overflow is a counted, logged rejection at the rules system (Framework.md "Rules algorithm").
/// Version: since Z2 Stage 3. A limit is part of a component's layout: changing one is a component version step with a
///   migration (Zenith2/Docs/Contracts.md "Compatibility policy per kind").
/// @endcontract
inline constexpr uint32 uGAMEPLAY_MAX_TAGS = 16;               // per entity, Zenith_GameplayTagsComponent
inline constexpr uint32 uGAMEPLAY_MAX_ATTRIBUTES = 16;         // per entity, Zenith_GameplayAttributesComponent
inline constexpr uint32 uGAMEPLAY_MAX_EFFECTS = 8;             // active effects per entity
inline constexpr uint32 uGAMEPLAY_MAX_MODIFIERS = 4;           // modifiers per effect
inline constexpr uint32 uGAMEPLAY_MAX_EFFECT_TAGS = 2;         // tags an effect grants while active
inline constexpr uint32 uGAMEPLAY_MAX_ABILITIES = 8;           // granted abilities per entity
inline constexpr uint32 uGAMEPLAY_MAX_ABILITY_TAGS = 2;        // per required / blocked / active tag list
inline constexpr uint32 uGAMEPLAY_MAX_TIMERS = 8;              // per entity
inline constexpr uint32 uGAMEPLAY_MAX_STATE_MACHINES = 4;      // per entity
inline constexpr uint32 uGAMEPLAY_MAX_ACTIONS = 8;             // per entity
inline constexpr uint32 uGAMEPLAY_ACTION_RETENTION_TICKS = 2;  // a finished action stays readable this many ticks
inline constexpr uint32 uGAMEPLAY_MAX_OWNER_HOPS = 8;          // Zenith_GameplayResolveOwningPlayer chain limit
inline constexpr uint32 uGAMEPLAY_MAX_TIMER_FIRES_PER_TICK = 64;
inline constexpr uint32 uGAMEPLAY_MAX_REQUEST_PAYLOAD_BYTES = 96;
inline constexpr uint32 uGAMEPLAY_INFINITE_TICKS = 0xFFFFFFFFu;  // a duration, repeat count or deadline that never ends
inline constexpr uint8 uGAMEPLAY_MAX_TAG_COUNT = 255;          // reference count of one tag on one entity

/// @contract enum Zenith_GameplayModifierOp
/// Ownership: Plain enumeration; the values of a modifier's op byte.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None; any other byte value makes the effect specification invalid (EFFECT_REJECT_REASON_INVALID_SPEC).
/// Version: since Z2 Stage 3 (declared by the API-only slice; used since Stage 4). Serialized; the values never move.
/// @endcontract
enum Zenith_GameplayModifierOp : uint8
{
	MODIFIER_OP_ADD = 1,        // current += magnitude × stacks
	MODIFIER_OP_MULTIPLY = 2,   // current *= magnitude ^ stacks
	MODIFIER_OP_OVERRIDE = 3,   // current = magnitude (the last override in evaluation order wins)
};

// The remaining enumerations of this header carry the same contract fields; each block names its value semantics.
enum Zenith_GameplayStackingPolicy : uint8
{
	STACKING_POLICY_INDEPENDENT = 0,   // every application is its own active-effect record
	STACKING_POLICY_REFRESH = 1,       // a second application of the same name refreshes the end tick; stacks unchanged
	STACKING_POLICY_STACK = 2,         // a second application adds one stack (up to m_uMaxStacks) and refreshes the end tick
	STACKING_POLICY_REJECT = 3,        // a second application while one is active is rejected
};

enum Zenith_GameplayEffectFlags : uint8
{
	EFFECT_FLAG_NONE = 0,
	EFFECT_FLAG_REMOVE_WITH_SOURCE = 1u << 0,      // removed when the source entity is destroyed
	EFFECT_FLAG_EXECUTE_PERIOD_ON_APPLY = 1u << 1, // a periodic effect executes once at application
};

enum Zenith_GameplayEffectRemoveReason : uint8
{
	EFFECT_REMOVE_REASON_EXPIRED = 1,
	EFFECT_REMOVE_REASON_REMOVED = 2,          // a RemoveEffect request
	EFFECT_REMOVE_REASON_SOURCE_DESTROYED = 3, // EFFECT_FLAG_REMOVE_WITH_SOURCE and the source is pending destroy or stale
};

enum Zenith_GameplayEffectRejectReason : uint8
{
	EFFECT_REJECT_REASON_FULL = 1,             // uGAMEPLAY_MAX_EFFECTS active records already
	EFFECT_REJECT_REASON_BLOCKED_TAG = 2,      // the target carries m_xBlockedTag
	EFFECT_REJECT_REASON_MISSING_TAG = 3,      // the target lacks m_xRequiredTag
	EFFECT_REJECT_REASON_STACKING = 4,         // STACKING_POLICY_REJECT with an active record of the name
	EFFECT_REJECT_REASON_NO_ATTRIBUTES = 5,    // modifiers present and the target has no Zenith.Gameplay.Attributes
	EFFECT_REJECT_REASON_INVALID_SPEC = 6,     // a field outside its range (Abilities.md "Effect specification")
	EFFECT_REJECT_REASON_NO_EFFECTS_COMPONENT = 7,   // a duration effect on a target without Zenith.Gameplay.Effects
};

enum Zenith_GameplayAbilityState : uint8
{
	ABILITY_STATE_READY = 0,
	ABILITY_STATE_ACTIVE = 1,
	ABILITY_STATE_COOLDOWN = 2,
};

enum Zenith_GameplayAbilityFlags : uint8
{
	ABILITY_FLAG_NONE = 0,
	ABILITY_FLAG_COOLDOWN_ON_END = 1u << 0,    // the cooldown starts when the activation ends, not when it starts
	ABILITY_FLAG_CANCEL_ON_BLOCKED = 1u << 1,  // an active activation is cancelled when a blocked tag is added
};

enum Zenith_GameplayAbilityEndReason : uint8
{
	ABILITY_END_REASON_ENDED = 1,      // an EndAbility request
	ABILITY_END_REASON_CANCELLED = 2,  // a CancelAbility request
	ABILITY_END_REASON_EXPIRED = 3,    // m_uMaxDurationTicks elapsed
	ABILITY_END_REASON_REVOKED = 4,    // a RevokeAbility request while active
	ABILITY_END_REASON_BLOCKED = 5,    // ABILITY_FLAG_CANCEL_ON_BLOCKED
};

enum Zenith_GameplayAbilityFailure : uint8
{
	ABILITY_FAILURE_NOT_GRANTED = 1,
	ABILITY_FAILURE_ALREADY_ACTIVE = 2,
	ABILITY_FAILURE_ON_COOLDOWN = 3,
	ABILITY_FAILURE_MISSING_TAG = 4,
	ABILITY_FAILURE_BLOCKED_TAG = 5,
	ABILITY_FAILURE_INSUFFICIENT_COST = 6,
	ABILITY_FAILURE_NO_COST_ATTRIBUTE = 7,
};

enum Zenith_GameplayActionKind : uint8
{
	ACTION_KIND_MANUAL = 1,            // finished only by CompleteAction, CancelAction or the deadline
	ACTION_KIND_DELAY = 2,             // succeeds at the deadline (a timer-free wait)
	ACTION_KIND_WAIT_TAG_ADDED = 3,    // succeeds when the subject carries m_xParam (hierarchical match)
	ACTION_KIND_WAIT_TAG_REMOVED = 4,  // succeeds when the subject carries no tag matching m_xParam
	ACTION_KIND_WAIT_STATE = 5,        // succeeds when machine m_xParam of the subject is in state m_xParam2
	ACTION_KIND_WAIT_ATTRIBUTE = 6,    // succeeds when attribute m_xParam's current value compares true against m_fValue
	ACTION_KIND_WAIT_ABILITY_END = 7,  // succeeds when ability m_xParam of the subject leaves ABILITY_STATE_ACTIVE
	ACTION_KIND_WAIT_TIMER = 8,        // succeeds when timer m_xParam of the subject fires
};

enum Zenith_GameplayActionState : uint8
{
	ACTION_STATE_RUNNING = 1,
	ACTION_STATE_SUCCEEDED = 2,
	ACTION_STATE_FAILED = 3,           // the subject became stale, or a FailAction request
	ACTION_STATE_CANCELLED = 4,
	ACTION_STATE_TIMED_OUT = 5,        // the deadline passed first
};

enum Zenith_GameplayCompare : uint8
{
	COMPARE_LESS = 1, COMPARE_LESS_EQUAL = 2, COMPARE_GREATER = 3, COMPARE_GREATER_EQUAL = 4,
	COMPARE_EQUAL = 5, COMPARE_NOT_EQUAL = 6,   // EQUAL and NOT_EQUAL compare the f32 bit patterns
};

enum Zenith_GameplayTimerFlags : uint8 { TIMER_FLAG_NONE = 0, TIMER_FLAG_PAUSED = 1u << 0 };
enum Zenith_GameplayPawnFlags : uint8 { PAWN_FLAG_NONE = 0, PAWN_FLAG_ALLOW_POSSESSION = 1u << 0, PAWN_FLAG_AUTO_POSSESS = 1u << 1 };

/// @contract enum Zenith_GameplayError
/// Ownership: Plain enumeration; the error of the module's start, attach and configuration steps.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: GAMEPLAY_ERROR_OUT_OF_MEMORY: a world state, queue or table allocation failed. GAMEPLAY_ERROR_REGISTRATION: a
///   type, command kind, system, resource or binding registration was rejected by its context. GAMEPLAY_ERROR_MISSING_TYPE:
///   an engine type the module reads by stable name (Zenith.Render.Camera with properties FovYDegrees and CutRequest,
///   Zenith.LocalTransform, Zenith.TransformHierarchy) is not registered or has a different property type.
///   GAMEPLAY_ERROR_INVALID_CONFIG: a gameplay.* configuration value is outside its range. The module maps each to the
///   Zenith_ModuleError of Zenith2/Docs/Architecture/CompositionRoot.md "Module interfaces" (REGISTRATION and MISSING_TYPE
///   to MODULE_ERROR_REGISTRATION, OUT_OF_MEMORY to MODULE_ERROR_OUT_OF_MEMORY, INVALID_CONFIG to MODULE_ERROR_REGISTRATION).
/// Version: since Z2 Stage 3; enumerators keep their meaning once shipped.
/// @endcontract
enum Zenith_GameplayError : uint8
{
	GAMEPLAY_ERROR_OUT_OF_MEMORY = 1,
	GAMEPLAY_ERROR_REGISTRATION = 2,
	GAMEPLAY_ERROR_MISSING_TYPE = 3,
	GAMEPLAY_ERROR_INVALID_CONFIG = 4,
};
constexpr const char* Zenith_ToString(Zenith_GameplayError eError);
```

Every enumeration above carries its own contract block in the header with the five fields of the first block (`Zenith2/Docs/Contracts.md` → "Contract blocks per type"); the listing abbreviates the repeated blocks to one comment.

## Gameplay time

Every absolute time a gameplay component stores — an effect's application and end, an ability's cooldown end, a timer's next fire, a state's entry, an action's start, deadline and finish — is a **gameplay tick**: g = the world's tick index + the `ContinuityTickBase` of the world state resource `Zenith.Runtime.WorldState` (`Zenith2/Docs/Design/GAME/WorldHost.md` → "World state resource"). The base is 0 in a startup world and is set by the world host at a world transition to the old world's base plus its last executed tick, so gameplay ticks continue across a swap and a carried-over entity's stored times keep their meaning with no rebase (`Zenith2/Docs/Design/GAME/WorldHost.md` → "Carry-over"); a world restored from a save gets the saved base. Throughout this family, "tick t" in a rule of a gameplay system means the gameplay tick of the tick being run, `Zenith_GameplayCurrentTick(xContext)` = `xContext.GetTickIndex() + pxWorldState->m_ulContinuityTickBase`, read once at the start of each system run; durations, intervals and cooldowns are tick counts and need no base. The Luau `Gameplay.GetTick()` binding returns the same value.

## Component and resource registration

`Zenith_Module_Gameplay_RegisterTypes(Zenith_TypeRegistry&)` registers, in this order, through `Zenith_TypeRegistry::BeginComponent<T>` / `BeginType<T>` (`Zenith2/Docs/Design/ECS/Reflect.md` → "Zenith_TypeBuilder"): the twelve components, the request-queue resource, the event types of § "Gameplay events", the command payload structs of § "The request queue", and the configuration keys of `Zenith2/Docs/Design/GAME.md` → "Configuration keys". Every component is registered at version 1 with `NetworkAuthority(NETWORK_AUTHORITY_SERVER)`, every property with the flags `PROPERTY_FLAG_SERIALIZED` (the default of a property declaration, `Zenith2/Docs/Design/ECS/Reflect.md` → "Zenith_TypeBuilder"; GAME sets no other flag and registers no transient component property), fixed arrays through the fixed-array traits of `Zenith2/Docs/Design/ECS/Reflect.md` → "Zenith_ReflectValueTraits header". The one derived property, `Zenith.Gameplay.Attributes` property 3 `Current`, is serialized like the rest (`Zenith2/Docs/Design/GAME/Abilities.md` → "Attribute component" gives the reason); ECS defines no inspector read-only flag and GAME asks for none, because an inspector edit of `Current` is harmless: the edit commits a newer row version, so the next rules pass re-evaluates the row (§ "Rules algorithm" step 7) and overwrites it.

| Stable name | C++ type (header) | Bytes | Properties (ID name) | Authority | Notes |
|---|---|---|---|---|---|
| `Zenith.Gameplay.Tags` | `Zenith_GameplayTagsComponent` (`Zenith_GameplayComponents.h`) | 84 | 1 Tags, 2 Counts, 3 Count | AINavigation | § "Tag component" |
| `Zenith.Gameplay.Attributes` | `Zenith_GameplayAttributesComponent` (`Zenith_GameplayAbilities.h`) | 328 | 1 Names, 2 Base, 3 Current (derived, serialized), 4 Min, 5 Max, 6 Count | AINavigation | `Zenith2/Docs/Design/GAME/Abilities.md` → "Attribute component" |
| `Zenith.Gameplay.Effects` | `Zenith_GameplayEffectsComponent` (`Zenith_GameplayAbilities.h`) | 904 | 1 Effects, 2 NextInstanceID, 3 Count | AINavigation | `Zenith2/Docs/Design/GAME/Abilities.md` → "Active effect records" |
| `Zenith.Gameplay.Abilities` | `Zenith_GameplayAbilitiesComponent` (`Zenith_GameplayAbilities.h`) | 648 | 1 Abilities, 2 Count | AINavigation | `Zenith2/Docs/Design/GAME/Abilities.md` → "Ability component" |
| `Zenith.Gameplay.Timers` | `Zenith_GameplayTimersComponent` | 264 | 1 Timers, 2 Count | AINavigation | § "Timers" |
| `Zenith.Gameplay.StateMachines` | `Zenith_GameplayStateMachinesComponent` | 104 | 1 Machines, 2 Count | AINavigation | § "State machines" |
| `Zenith.Gameplay.Actions` | `Zenith_GameplayActionsComponent` | 520 | 1 Actions, 2 NextActionID, 3 Count | AINavigation | § "Gameplay actions" |
| `Zenith.Gameplay.Player` | `Zenith_GameplayPlayerComponent` | 24 | 1 Pawn, 2 CameraDirector, 3 PossessionCount, 4 PlayerSlot, 5 Flags | 1 and 3: AINavigation; 2, 4, 5: CommandsOnly | § "Player and pawn components" |
| `Zenith.Gameplay.Pawn` | `Zenith_GameplayPawnComponent` | 16 | 1 PossessedBy, 2 Flags | 1: AINavigation; 2: CommandsOnly | same |
| `Zenith.Gameplay.Owner` | `Zenith_GameplayOwnerComponent` | 16 | 1 Owner, 2 Team | CommandsOnly | § "Ownership" |
| `Zenith.Gameplay.CameraRig` | `Zenith_GameplayCameraRigComponent` (`Zenith_GameplayCameras.h`) | 144 | per `Zenith2/Docs/Design/GAME/Cameras.md` → "Zenith_GameplayCameraRigComponent" | per property there | cameras |
| `Zenith.Gameplay.CameraDirector` | `Zenith_GameplayCameraDirectorComponent` (`Zenith_GameplayCameras.h`) | 120 | per `Zenith2/Docs/Design/GAME/Cameras.md` → "Zenith_GameplayCameraDirectorComponent" | per property there (property 18 `FollowPawn` is written by `Gameplay.Rules`) | cameras |
| `Zenith.Gameplay.RequestQueue` | `Zenith_GameplayRequestQueue` (`Internal/Zenith_GameplayRequestQueue.h`) | — | — (`TYPE_KIND_RESOURCE`) | resource owning phase AINavigation | § "Queue record format"; not serialized |

The authority column is the census row of `Zenith2/Docs/Design/GAME/PhaseContract.md` → "Census of signed-off designs". A script writes a rule-owned property only through the request bindings (§ "Command kinds and payloads"); a direct write from a behaviour or chunk system is `SCHEDULE_ERROR_AUTHORITY_VIOLATION` at schedule compile, because the authority is phase 7 and scripts run in phases 2 and 10 ([Z2-N-ARCH-483]).

## Gameplay tags

### Tag component

A gameplay tag is an interned dotted name (`Zenith_StringID`, text such as `Status.Stunned` or `Ability.Dash.Active`): segments of `[A-Za-z0-9_]+` separated by single dots, 1 to 127 bytes. The tag component holds an entity's tags with a reference count per tag, so independent sources (a script, two effects, an active ability) can each add and remove the same tag:

```cpp
// Zenith2/Engine/ZenithGameplay/Zenith_GameplayComponents.h
/// @contract value Zenith_GameplayTagsComponent
/// Ownership: Stored by the world's ECS backend as component "Zenith.Gameplay.Tags".
/// Thread affinity: IMMUTABLE as a value; members non-const: an ECS component the store relocates and systems write
///   property by property; rows follow the world's phase access rules (authority AINavigation).
/// Invalidation: None.
/// Units: m_auCounts are reference counts (1 to 255) of the tag at the same index.
/// Errors: None; a 17th distinct tag is rejected by the rules system and counted (gameplay.tags.full).
/// Version: since Z2 Stage 3; type version 1; property IDs 1 to 3 are permanent.
/// @endcontract
struct Zenith_GameplayTagsComponent
{
	Zenith_StringID m_axTags[uGAMEPLAY_MAX_TAGS];    // 1 Tags: indices [0, m_uCount) are live, in insertion order
	uint8 m_auCounts[uGAMEPLAY_MAX_TAGS] = {};      // 2 Counts
	uint8 m_uCount = 0;                             // 3 Count
	uint8 m_auReserved[3] = {};
	static constexpr Zenith_PropertyID uPROP_TAGS{ 1 };
	static constexpr Zenith_PropertyID uPROP_COUNTS{ 2 };
	static constexpr Zenith_PropertyID uPROP_COUNT{ 3 };
	static const Zenith_TypeID xTYPE_ID;             // Zenith_MakeTypeID("Zenith.Gameplay.Tags")
	static constexpr bool bZENITH_TRIVIALLY_RELOCATABLE = true;
};
static_assert(sizeof(Zenith_GameplayTagsComponent) == 84);
```

Live entries are kept dense and in insertion order: removal shifts the following entries left by one (never a swap), so the order a script observes is the order of the requests that added the tags, which is merge order and therefore identical on every run (`Zenith_StringID` values are never used to order, `Zenith2/Docs/Design/CORE/Text.md` → "String interning").

### Tag matching

```cpp
// Zenith2/Engine/ZenithGameplay/Zenith_GameplayMath.h
/// @contract functions GameplayTagMatching
/// Ownership: Pure functions over the caller's values; borrow the string table for the call.
/// Thread affinity: ANY_THREAD. Zenith_StringTable::GetText is lock-free (Zenith2/Docs/Design/CORE/Text.md "String interning").
/// Invalidation: None.
/// Units: None.
/// Errors: None; an empty query matches nothing; an ID the table has not issued is ZENITH_CHECK inside GetText.
/// Version: since Z2 Stage 3.
/// @endcontract
bool Zenith_GameplayTagMatches(const Zenith_StringTable& xStrings, Zenith_StringID xTag, Zenith_StringID xQuery);
bool Zenith_GameplayTagsHasMatch(const Zenith_StringTable& xStrings, const Zenith_GameplayTagsComponent& xTags,
	Zenith_StringID xQuery);
uint32 Zenith_GameplayTagsFindExact(const Zenith_GameplayTagsComponent& xTags, Zenith_StringID xTag);   // index or uGAMEPLAY_MAX_TAGS
bool Zenith_GameplayIsValidTagText(Zenith_StringView strText);
```

`Zenith_GameplayTagMatches(t, q)` is true exactly when q is not empty and either `t == q`, or `text(t)` is longer than `text(q)`, starts with `text(q)` byte for byte, and the byte after that prefix is `.`. So `Status.Stunned` matches the queries `Status.Stunned` and `Status` but not `Status.Stun` or `Stunned`. `Zenith_GameplayTagsHasMatch` tests the live entries in index order and returns at the first match; its cost is O(uGAMEPLAY_MAX_TAGS × length).

### Tag reference counting

Applied by the rules system to one target row (§ "Rules algorithm" step 2):

| Operation | Effect | Event |
|---|---|---|
| add n of tag t (n 1 to 255), t present at index i | `m_auCounts[i] = min(255, m_auCounts[i] + n)`; a saturation counts `gameplay.tags.count_saturated` | none (presence unchanged) |
| add n of t, t absent, `m_uCount < 16` | append t at index `m_uCount` with count n; `m_uCount += 1` | `Zenith.Gameplay.TagChanged { Entity, Tag = t, Added = true, Count = n }` |
| add n of t, t absent, `m_uCount == 16` | nothing; `gameplay.tags.full` + 1; log `gameplay: entity <simID>: tag '<t>' rejected, 16 tags held` (once per entity per tick) | `Zenith.Gameplay.TagRejected { Entity, Tag }` |
| remove n of t, t present, count > n | `m_auCounts[i] -= n` | none |
| remove n of t (or all, n = 255 with the `ALL` flag), t present, count ≤ n | shift entries i+1 … left; `m_uCount -= 1` | `TagChanged { Added = false, Count = 0 }` |
| remove of t absent | nothing; `gameplay.tags.remove_absent` + 1 | none |

Sources that grant tags (an active effect's `m_axGrantedTags`, an active ability's `m_axActiveTags`) add one reference at activation and remove one at deactivation, so a script's own reference and theirs never cancel each other. An entity without `Zenith.Gameplay.Tags` that receives a tag request is skipped (`gameplay.requests.missing_component` + 1); the component is added by authoring or by an `AddComponent` command, never implicitly.

## Gameplay events

### Event types

Every GAME event is a registered `TYPE_KIND_EVENT` type (`Zenith2/Docs/Design/ECS/Schemas.md` → "Event schemas" admits C++-registered events through the same registry). The `Zenith.Gameplay.*` events are emitted by `Gameplay.Rules` (phase 7) or `Gameplay.WorldEvents` (phase 1), so they are delivered in phase 9 of the same tick and read by Late behaviours in phase 10 ([Z2-N-ARCH-478]); each consumes one record of the event capacity Z2-B-GAME-003 at the emitting merge.

```cpp
// Zenith2/Engine/ZenithGameplay/Zenith_GameplayEvents.h
// One struct per event type; property IDs follow member order from 1 (reserved members are not properties). Each struct
// carries the value-type contract block whose fields § "Value type contract blocks" gives.
struct Zenith_GameplayTagChangedEvent       { Zenith_EntityHandle m_xEntity; Zenith_StringID m_xTag; uint8 m_bAdded; uint8 m_uCount; };
struct Zenith_GameplayTagRejectedEvent      { Zenith_EntityHandle m_xEntity; Zenith_StringID m_xTag; };
struct Zenith_GameplayAttributeChangedEvent { Zenith_EntityHandle m_xEntity; Zenith_StringID m_xAttribute; float m_fOldValue; float m_fNewValue; };
struct Zenith_GameplayEffectAppliedEvent    { Zenith_EntityHandle m_xEntity; Zenith_EntityHandle m_xSource; Zenith_StringID m_xEffect; uint32 m_uInstanceID; uint8 m_uStacks; };
struct Zenith_GameplayEffectRemovedEvent    { Zenith_EntityHandle m_xEntity; Zenith_StringID m_xEffect; uint32 m_uInstanceID; uint8 m_eReason; };
struct Zenith_GameplayEffectRejectedEvent   { Zenith_EntityHandle m_xEntity; Zenith_StringID m_xEffect; uint8 m_eReason; };
struct Zenith_GameplayAbilityActivatedEvent { Zenith_EntityHandle m_xEntity; Zenith_EntityHandle m_xInstigator; Zenith_StringID m_xAbility; uint32 m_uActivationID; uint8 m_uLevel; };
struct Zenith_GameplayAbilityEndedEvent     { Zenith_EntityHandle m_xEntity; Zenith_StringID m_xAbility; uint32 m_uActivationID; uint8 m_eReason; };
struct Zenith_GameplayAbilityFailedEvent    { Zenith_EntityHandle m_xEntity; Zenith_StringID m_xAbility; uint8 m_eFailure; };
struct Zenith_GameplayAbilityReadyEvent     { Zenith_EntityHandle m_xEntity; Zenith_StringID m_xAbility; };
struct Zenith_GameplayTimerElapsedEvent     { Zenith_EntityHandle m_xEntity; Zenith_StringID m_xTimer; uint32 m_uFiredCount; uint8 m_bFinal; };
struct Zenith_GameplayStateChangedEvent     { Zenith_EntityHandle m_xEntity; Zenith_StringID m_xMachine; Zenith_StringID m_xFrom; Zenith_StringID m_xTo; };
struct Zenith_GameplayActionFinishedEvent   { Zenith_EntityHandle m_xEntity; Zenith_StringID m_xAction; uint32 m_uActionID; uint8 m_eState; };
struct Zenith_GameplayPossessionChangedEvent { Zenith_EntityHandle m_xPlayer; Zenith_EntityHandle m_xOldPawn; Zenith_EntityHandle m_xNewPawn; };
struct Zenith_GameplayRequestsDroppedEvent  { uint32 m_uCount; };
struct Zenith_GameplayWorldStartedEvent     { uint64 m_ulFirstTick; uint64 m_ulWorldSeed; uint8 m_eOrigin; uint8 m_auReserved[7]; };   // m_eOrigin: Zenith_WorldOrigin
struct Zenith_GameplayMessageEvent          { Zenith_StringID m_xChannel; Zenith_EntityHandle m_xInstigator; Zenith_EntityHandle m_xTarget; float m_fMagnitude; Zenith_StringID m_xPayload; };
```

| Stable name | Struct | Emitted by, on simulation ID | When |
|---|---|---|---|
| `Zenith.Gameplay.TagChanged` | `Zenith_GameplayTagChangedEvent` | `Gameplay.Rules`, the entity | a tag's presence changes |
| `Zenith.Gameplay.TagRejected` | `…TagRejectedEvent` | `Gameplay.Rules`, the entity | a 17th distinct tag |
| `Zenith.Gameplay.AttributeChanged` | `…AttributeChangedEvent` | `Gameplay.Rules`, the entity | an attribute's current value changes bitwise |
| `Zenith.Gameplay.EffectApplied`, `.EffectRemoved`, `.EffectRejected` | as named | `Gameplay.Rules`, the target | `Zenith2/Docs/Design/GAME/Abilities.md` → "Effect application algorithm" |
| `Zenith.Gameplay.AbilityActivated`, `.AbilityEnded`, `.AbilityFailed`, `.AbilityReady` | as named | `Gameplay.Rules`, the owner | `Zenith2/Docs/Design/GAME/Abilities.md` → "Ability activation algorithm" |
| `Zenith.Gameplay.TimerElapsed` | `…TimerElapsedEvent` | `Gameplay.Rules`, the owner | every fire |
| `Zenith.Gameplay.StateChanged` | `…StateChangedEvent` | `Gameplay.Rules`, the owner | a machine's state changes |
| `Zenith.Gameplay.ActionFinished` | `…ActionFinishedEvent` | `Gameplay.Rules`, the owner | an action leaves `ACTION_STATE_RUNNING` |
| `Zenith.Gameplay.PossessionChanged` | `…PossessionChangedEvent` | `Gameplay.Rules`, the player | a possession or unpossession takes effect |
| `Zenith.Gameplay.RequestsDropped` | `…RequestsDroppedEvent` | `Gameplay.Rules`, simulation ID 0 | the previous playbacks dropped requests (§ "Playback into the queue") |
| `Zenith.Gameplay.WorldStarted` | `…WorldStartedEvent` | `Gameplay.WorldEvents`, simulation ID 0 | the world's first tick, for every origin, carrying it (§ "The world events system") |
| `Zenith.Gameplay.Message` | `…MessageEvent` | scripts (`Gameplay.SendMessage`, `Zenith2/Docs/Design/GAME/Scripting.md` → "Binding functions"), any C++ system that declares it | a game-defined message |

Event emission order inside one tick is merge order (system order, simulation ID, invocation ordinal, sequence; `Zenith2/Docs/Design/ECS/WorldAPI.md` → "Zenith_CommandBuffer header"): `Gameplay.Rules` records each event into the command buffer of the entity named in the table (`Zenith_SystemContext::Events(ulSimulationID)`), with one invocation ordinal per entity and sequences in the order the rules algorithm produces them, so two runs with different worker counts deliver identical sequences.

### The message event

`Zenith.Gameplay.Message` is the generic routed message: a channel tag, an instigator, a target, a magnitude and a free name payload, enough for most game-defined notifications (a hit, a door used, a quest objective) without a new event schema. Its readers filter by channel with hierarchical matching through the Luau helper `Messages.OnChannel` (`Zenith2/Docs/Design/GAME/Scripting.md` → "Message helpers"). A game that needs richer payloads declares its own event schema in `Games/<Name>/Schemas/` (`Zenith2/Docs/Architecture/DataDefinedComponents.md` → "The schema file"); GAME's message is a convenience, never the only path.

## State machines

A reusable state primitive: up to four named machines per entity, each holding one current state name. The machine records the transition history depth of one (the previous state) and the tick it entered the current state; hierarchical and guarded state logic is the Luau helper of `Zenith2/Docs/Design/GAME/Scripting.md` → "State machine helper", built on this component.

```cpp
// Zenith2/Engine/ZenithGameplay/Zenith_GameplayComponents.h
struct Zenith_GameplayStateMachineRecord      // 24 bytes
{
	uint64 m_ulEnteredTick = 0;   // gameplay tick (§ "Gameplay time") at which m_xState became current
	Zenith_StringID m_xMachine;    // the machine's name, unique per entity
	Zenith_StringID m_xState;      // current state; empty = no state yet
	Zenith_StringID m_xPrevious;   // the state before m_xState; empty before the first transition
	uint32 m_uTransitionCount = 0;
};
struct Zenith_GameplayStateMachinesComponent  // 104 bytes
{
	Zenith_GameplayStateMachineRecord m_axMachines[uGAMEPLAY_MAX_STATE_MACHINES];   // 1 Machines
	uint8 m_uCount = 0;                                                            // 2 Count
	uint8 m_auReserved[7] = {};
	static const Zenith_TypeID xTYPE_ID;      // "Zenith.Gameplay.StateMachines"
};
static_assert(sizeof(Zenith_GameplayStateMachinesComponent) == 104);
```

Requests (`Gameplay.State` command kind): `SET(machine, state)` — finds the machine by name or appends it (a fifth machine is rejected, `gameplay.state.full`); if the current state equals `state` nothing changes and no event is emitted (`gameplay.state.redundant` + 1); otherwise `m_xPrevious = m_xState`, `m_xState = state`, `m_ulEnteredTick = t`, `m_uTransitionCount += 1`, event `StateChanged { From, To }`. `RESET(machine)` — sets the state to empty with the same bookkeeping (event `To` empty). `REMOVE(machine)` — removes the record (shift left), event `To` empty. Several `SET` requests of one machine in one tick apply in queue order and each emits its event, so a script that reads every `StateChanged` sees the full path.

## Timers

```cpp
struct Zenith_GameplayTimerRecord             // 32 bytes
{
	uint64 m_ulFireTick = 0;       // gameplay tick (§ "Gameplay time") of the next fire; while paused, the ticks remaining
	Zenith_StringID m_xName;        // unique per entity
	uint32 m_uIntervalTicks = 0;   // 0 = one shot
	uint32 m_uRemainingRepeats = 0;// fires left after the next one; uGAMEPLAY_INFINITE_TICKS = forever
	uint32 m_uFiredCount = 0;
	uint8 m_uFlags = 0;            // Zenith_GameplayTimerFlags
	uint8 m_auReserved[7] = {};
};
struct Zenith_GameplayTimersComponent         // 264 bytes
{
	Zenith_GameplayTimerRecord m_axTimers[uGAMEPLAY_MAX_TIMERS];   // 1 Timers
	uint8 m_uCount = 0;                                           // 2 Count
	uint8 m_auReserved[7] = {};
	static const Zenith_TypeID xTYPE_ID;      // "Zenith.Gameplay.Timers"
};
static_assert(sizeof(Zenith_GameplayTimersComponent) == 264);
```

Time is counted in ticks so it is deterministic and replayable; the Luau bindings convert seconds with `ceil(seconds × tickHz)` from the world's fixed tick rate (`sim.tickHz`), a duration of 0 seconds becoming 1 tick.

| Request (`Gameplay.Timer`) | Effect at the rules system, tick t |
|---|---|
| `START(name, delayTicks ≥ 1, intervalTicks ≥ 0, repeats)` | replaces a timer of the same name or appends one (a ninth is rejected, `gameplay.timers.full`): `m_ulFireTick = t + delayTicks`, interval and repeats as given (a one-shot passes `intervalTicks = 0`, `repeats = 0`), `m_uFiredCount = 0`, flags cleared |
| `STOP(name)` | removes it (shift left); no event |
| `PAUSE(name)` | if running: `m_ulFireTick = max(1, m_ulFireTick − t)` (remaining), set `TIMER_FLAG_PAUSED` |
| `RESUME(name)` | if paused: `m_ulFireTick = t + m_ulFireTick`, clear the flag |

Firing (§ "Rules algorithm" step 3): for each timer, in index order, while it is not paused, `m_ulFireTick ≤ t` and fewer than `uGAMEPLAY_MAX_TIMER_FIRES_PER_TICK` fires happened this tick: `m_uFiredCount += 1`; final = (`m_uIntervalTicks == 0` or `m_uRemainingRepeats == 0`); emit `TimerElapsed { Timer, FiredCount, Final }` and append (owner simulation ID, timer name) to the node arena's fire list, which the `WAIT_TIMER` probes of § "Action evaluation" read; if final remove the timer, else `m_ulFireTick += m_uIntervalTicks` and decrement `m_uRemainingRepeats` unless infinite. When the fire limit stops a timer whose next fire is still `≤ t`, its `m_ulFireTick` becomes `t + m_uIntervalTicks` and `gameplay.timers.fires_clamped` + 1 (a pathological interval of 1 tick with 64 overdue fires after a long pause).

## Gameplay actions

### Action component

A gameplay action is an entity-scoped latent operation with an ID, a kind, a state and an outcome — the engine half of the async gameplay actions of inventory item Z2-I-GAME-19. A behaviour starts one, then awaits it with the Luau helper `Async.Await` (`Zenith2/Docs/Design/GAME/Scripting.md` → "Async helpers"), which yields until the action's record leaves `ACTION_STATE_RUNNING`. The rules system evaluates every running action once per tick, so completion is detected deterministically in phase 7 and observed by the awaiting behaviour in its next segment.

```cpp
struct Zenith_GameplayActionRecord            // 64 bytes
{
	Zenith_EntityHandle m_xSubject;   // the observed entity; null = the owning entity
	uint64 m_ulStartTick = 0;
	uint64 m_ulDeadlineTick = 0;     // 0 = none; reaching it finishes the action TIMED_OUT (DELAY: SUCCEEDED)
	uint64 m_ulFinishedTick = 0;     // 0 while running
	uint32 m_uActionID = 0;          // per entity, from m_uNextActionID, never 0
	Zenith_StringID m_xName;          // the script's label for the action (may be empty)
	Zenith_StringID m_xParam;         // tag, machine, attribute, ability or timer name per kind
	Zenith_StringID m_xParam2;        // the awaited state (ACTION_KIND_WAIT_STATE)
	float m_fValue = 0.0f;            // the comparison operand (ACTION_KIND_WAIT_ATTRIBUTE)
	uint8 m_eKind = 0;               // Zenith_GameplayActionKind
	uint8 m_eState = 0;              // Zenith_GameplayActionState
	uint8 m_eCompare = 0;            // Zenith_GameplayCompare
	uint8 m_uFlags = 0;              // reserved, 0
	uint32 m_uRecordedActivationCount = 0;   // ACTION_KIND_WAIT_ABILITY_END: the ability's m_uActivationCount at the start
	uint8 m_auReserved[4] = {};
};
static_assert(sizeof(Zenith_GameplayActionRecord) == 64);
struct Zenith_GameplayActionsComponent        // 520 bytes
{
	Zenith_GameplayActionRecord m_axActions[uGAMEPLAY_MAX_ACTIONS];   // 1 Actions
	uint32 m_uNextActionID = 1;                                      // 2 NextActionID
	uint8 m_uCount = 0;                                              // 3 Count
	uint8 m_auReserved[3] = {};
	static const Zenith_TypeID xTYPE_ID;      // "Zenith.Gameplay.Actions"
};
static_assert(sizeof(Zenith_GameplayActionsComponent) == 520);   // 8 × 64 + 4 + 1 + 3
```

The action ID is allocated when the start request is applied by the rules system, after the script that recorded it has committed, so scripts never hold an ID before it exists: a start request carries a script-chosen, non-empty `m_xName` that must be unique among the owner's running actions, and `Async.Await(ctx, owner, name)` finds the record by that name. A start whose name equals a running action's name is rejected (`gameplay.actions.duplicate_name`; the earlier action keeps running), and a start on a full component is rejected (`gameplay.actions.full`); both emit `ActionFinished { State = FAILED, ActionID = 0 }` with the requested name, so the awaiting script observes the failure. The ID exists for C++ readers and for `ActionFinished` consumers that track several actions of one name over time.

### Action evaluation

At tick t, for each running action in index order (§ "Rules algorithm" step 6), with S the subject (null = owner):

| Kind | Succeeds when | Fails when |
|---|---|---|
| `MANUAL` | a `COMPLETE` request names it | a `FAIL` request names it |
| `DELAY` | `t ≥ m_ulDeadlineTick` (finishes `SUCCEEDED`, not `TIMED_OUT`) | — |
| `WAIT_TAG_ADDED` | `Zenith_GameplayTagsHasMatch(S.Tags, m_xParam)` | S stale, pending destroy or without `Zenith.Gameplay.Tags` |
| `WAIT_TAG_REMOVED` | no tag of S matches `m_xParam` (an S without the tags component succeeds) | S stale or pending destroy |
| `WAIT_STATE` | S's machine `m_xParam` has state `m_xParam2` | S stale; S without `Zenith.Gameplay.StateMachines` |
| `WAIT_ATTRIBUTE` | `compare(v, m_fValue)`, v the attribute's evaluated value (below) | S stale; the attribute absent |
| `WAIT_ABILITY_END` | S's ability `m_xParam` is not `ABILITY_STATE_ACTIVE`, or its activation count differs from the count the start recorded (the start stores `m_uActivationCount` in the integer field `m_uRecordedActivationCount`; `m_fValue` stays the `WAIT_ATTRIBUTE` operand alone, because a count stored as a float bit pattern is a denormal below 2²³ and reads as 0 wherever the reflected `f32` is handled as a value under the flush-to-zero environment of `Zenith2/Docs/Contracts.md`) | at the action's start the ability is not active (the action finishes `FAILED` in the tick it starts, so an activation that failed earlier in the same drain never reads as a completed ability); S stale; the ability not granted |
| `WAIT_TIMER` | S's timer `m_xParam` fired this tick (a `TimerElapsed` emitted for it in step 3 of this tick) or no longer exists | S stale |

Order of checks per action: a `CANCEL`, `COMPLETE` or `FAIL` request processed this tick (step 2) has already finished it; otherwise the success condition; otherwise the failure condition; otherwise the deadline (`m_ulDeadlineTick != 0 && t ≥ m_ulDeadlineTick` → `TIMED_OUT`). Finishing sets `m_eState`, `m_ulFinishedTick = t` and emits `ActionFinished`. The conditions read state as it stands after steps 2 to 5 of the same tick, so an action waiting for a tag that a request of this tick added succeeds in this tick. For attributes that state is the evaluated value: step 7 refreshes the stored `Current` only at the end of the pass, so `WAIT_ATTRIBUTE` (like the ability cost check, `Zenith2/Docs/Design/GAME/Abilities.md` → "Ability activation algorithm") reads v = `Zenith_GameplayEvaluateAttribute(name, Base, Min, Max, S's effects row or null, bNonFinite)` over S's row as it stands — the value step 7 will store — never the stored `Current`; the evaluation is pure, counts nothing (step 7 alone counts `gameplay.attributes.non_finite`) and reads only the current entity's attributes and effects rows.

A subject other than the owner is read without a cross-entity access (§ "Row ownership in the rules node"): every component the conditions read is written by `Gameplay.Rules` in phase 7, so its columns are own-row-only for the node. Step 6 therefore runs in three passes:

1. **6a, owners.** Over the actions query (each row the current entity): retention removal (§ "Action retention"); for each running action whose subject is null or the owner, evaluate the condition now from the owner's row; for each running action with a foreign subject S, first `Resolve` S (the entity table, not a column, § "Row ownership in the rules node" item 4): a stale S, an S of another world (its handle's world tag differs), and a pending-destroy S of a kind whose failure column names pending destroy (`WAIT_TAG_ADDED`, `WAIT_TAG_REMOVED`) get the preset outcome `FAILS` with no probe — no simulation ID is needed, because no row of S is read; every other S (live, or pending destroy for the other kinds, whose rows ECS keeps readable until the end of the tick) gets a probe `{ S's simulation ID, owner's simulation ID, action index, kind, m_xParam, m_xParam2, m_fValue, m_eCompare, the recorded activation count }` in the node arena. An owner with no probe is finished at once, every action in index order (a preset outcome is applied in its place in that order); an owner with at least one probe is deferred to 6c whole, so its event sequence is the one an in-place evaluation would produce.
2. **6b, subjects.** Probes sorted by (S's simulation ID, owner's simulation ID, action index); for each S (every probed S resolved in 6a, so it has a simulation ID): make S current (`xContext.Events(S's simulation ID)`), then `GetChunkOfEntity` on the kind's query slot for S — an S outside the slot's query (without the component the kind reads) answers by the table: `WAIT_TAG_REMOVED` and `WAIT_TIMER` succeed (no tag of S matches; the timer no longer exists), every other kind fails — evaluate each of S's probes from S's own row and the fire list, and store the outcome (`SUCCEEDS`, `FAILS`, `NEITHER`) in the probe.
3. **6c, deferred owners.** In ascending owner simulation ID: make the owner current, `GetChunkOfEntity(actions slot, owner)`, and finish its running actions in index order — an own-subject action from its own row, a foreign-subject one from its probe's outcome or its preset outcome — then the deadline rule.

No row other than the current entity's is ever read or written, and the outcome is the same as the single in-place pass the table describes, because no step-6 write changes any state a condition reads.

### Action retention

A finished record stays in the component until `t ≥ m_ulFinishedTick + uGAMEPLAY_ACTION_RETENTION_TICKS`, then is removed (shift left) at the start of step 6, so a behaviour of either script phase observes the outcome in its next segment whichever phase it runs in. `Async.Await` treats a name with no record (retention elapsed, or never started) as finished with state `"Expired"`. An `ACK(name)` request removes a finished record at once.

## Players and possession

### Player and pawn components

```cpp
struct Zenith_GameplayPlayerComponent         // 24 bytes
{
	Zenith_EntityHandle m_xPawn;            // 1 Pawn (AINavigation): the possessed pawn, null when none
	Zenith_EntityHandle m_xCameraDirector;  // 2 CameraDirector (CommandsOnly): the director entity serving this player
	uint32 m_uPossessionCount = 0;         // 3 PossessionCount (AINavigation)
	uint16 m_uPlayerSlot = 0;              // 4 PlayerSlot (CommandsOnly): 0 is the local player
	uint8 m_uFlags = 0;                    // 5 Flags (CommandsOnly): reserved, 0
	uint8 m_uReserved = 0;
	static const Zenith_TypeID xTYPE_ID;    // "Zenith.Gameplay.Player"
};
struct Zenith_GameplayPawnComponent           // 16 bytes
{
	Zenith_EntityHandle m_xPossessedBy;     // 1 PossessedBy (AINavigation)
	uint8 m_uFlags = PAWN_FLAG_ALLOW_POSSESSION;   // 2 Flags (CommandsOnly): Zenith_GameplayPawnFlags
	uint8 m_auReserved[7] = {};
	static const Zenith_TypeID xTYPE_ID;    // "Zenith.Gameplay.Pawn"
};
```

Single-player production with local multiplayer excluded (Z2-D-076): exactly one player entity per world carries `m_uPlayerSlot == 0`; a second slot-0 player is an authoring error logged at the world's first tick (`gameplay: two players hold slot 0: <simID A>, <simID B>; the higher simulation ID is ignored`) and every request naming the ignored one is dropped. Slots 1 and above are reserved for NET's Stage 7 replication (the deferred requirement "Player slots and ownership under replication" of `Zenith2/Docs/Requirements/GAME.md`).

### Ownership

```cpp
struct Zenith_GameplayOwnerComponent          // 16 bytes
{
	Zenith_EntityHandle m_xOwner;   // 1 Owner (CommandsOnly): the owning player, pawn or any entity
	uint16 m_uTeam = 0;            // 2 Team (CommandsOnly): a game-defined team number; 0 = none
	uint8 m_auReserved[6] = {};
	static const Zenith_TypeID xTYPE_ID;    // "Zenith.Gameplay.Owner"
};

/// @contract functions GameplayOwnership
/// Ownership: Pure function over the caller's resolver; owns nothing.
/// Thread affinity: ANY_THREAD within a node that declared reads of Zenith.Gameplay.Owner and Zenith.Gameplay.Player.
/// Invalidation: None.
/// Units: None.
/// Errors: Returns the null handle for a chain without a player, a stale link, or a chain longer than
///   uGAMEPLAY_MAX_OWNER_HOPS (counted gameplay.owner.chain_too_long by the caller).
/// Version: since Z2 Stage 3.
/// @endcontract
using Zenith_PfnGameplayReadOwner = bool (*)(void* pUserData, Zenith_EntityHandle xEntity, Zenith_EntityHandle& xOwnerOut,
	bool& bIsPlayerOut);
Zenith_EntityHandle Zenith_GameplayResolveOwningPlayer(Zenith_EntityHandle xEntity, Zenith_PfnGameplayReadOwner pfnRead,
	void* pUserData);
```

Ownership answers "which player is responsible for this entity" (a projectile owned by a pawn owned by the player resolves to the player). `Zenith_GameplayResolveOwningPlayer` follows `Owner` links from the entity, returning the first entity that carries `Zenith.Gameplay.Player`, at most `uGAMEPLAY_MAX_OWNER_HOPS` links; the entity itself counts as hop 0. The Luau helper `Ownership.OwningPlayer(ctx, entity)` exposes it (`Zenith2/Docs/Design/GAME/Scripting.md` → "Read helpers"). Ownership is gameplay metadata; in Stage 7 NET maps the owning player to `NETWORK_AUTHORITY_OWNER_CLIENT` (`Zenith2/Docs/Architecture.md` → "Multiplayer foundations"; the same deferred requirement).

### Possession algorithm

Requests (`Gameplay.Possess` command kind): `POSSESS(player P, pawn N)`, `UNPOSSESS(player P)`, and the listener's `NOTIFY_UNPOSSESSED(player P, pawn N)` (op 3, recorded only by GAME's lifecycle listener, below). A possession touches up to four entities — P, N, the pawn O that P held, the player Q that held N — and the rules node writes only the current entity's row (§ "Row ownership in the rules node"), so the request is applied as a chain of follow-up records on the node arena's worklist, each applied with its own target current, depth first in the order appended, before the next queued request:

1. **P's record** (P current). Validate P (it must carry `Zenith.Gameplay.Player` and not be the ignored second slot-0 player) and, for `POSSESS`, N (resolves, carries `Zenith.Gameplay.Pawn`, and its `Flags` — a `CommandsOnly` property, read through the node's `ReadAnyRow` declaration — has `PAWN_FLAG_ALLOW_POSSESSION`); otherwise drop with `gameplay.possession.rejected` + 1 and the log line `gameplay: possession request of player <simID> rejected: <reason>` with reason `no player component`, `no pawn component`, `pawn does not allow possession` or `stale handle`. `POSSESS` where `P.Pawn == N`: no change, no event (`gameplay.possession.redundant` + 1). Otherwise O = `P.Pawn`; write `P.Pawn = N` (null for `UNPOSSESS`), `P.PossessionCount += 1`; emit `PossessionChanged { Player = P, OldPawn = O, NewPawn = N }` on P; append `RELEASE(O, P)` when O is not null, then `CLAIM(N, P)` for `POSSESS`.
2. **`RELEASE(O, P)`** (O current). If O still carries the pawn component and `O.PossessedBy == P`: `O.PossessedBy = null`.
3. **`CLAIM(N, P)`** (N current). Q = `N.PossessedBy`; `N.PossessedBy = P`; when Q is neither null nor P, append `DROP(Q, N)`.
4. **`DROP(Q, N)`** (Q current). If `Q.Pawn == N`: `Q.Pawn = null`, `Q.PossessionCount += 1`, emit `PossessionChanged { Player = Q, OldPawn = N, NewPawn = null }` on Q.

A follow-up record is 32 bytes (`{ op, target simulation ID, other simulation ID, handles }`) and a possession produces at most three, so the worklist never holds more than three records at once. The two `PossessionChanged` events of a steal are on two entities' buffers, delivered in merge order (ascending simulation ID of the emitting entity), not in the order of the steps. The camera follows the new pawn through step 9 of § "Rules algorithm", which writes the director's `FollowPawn` in the same pass (`Zenith2/Docs/Design/GAME/Cameras.md` → "Rig selection" rule 4).

Auto-possession: at a world's first tick (step 8 of § "Rules algorithm"), unless the world's origin is `WORLD_ORIGIN_SAVE_LOAD` (a restored world already holds the saved possession), (a) a pass over the pawns query appends every pawn with `PAWN_FLAG_AUTO_POSSESS` and a null `PossessedBy` (each read as the current row) to an arena list in ascending simulation ID; (b) a pass over the players query takes, for each player in ascending simulation ID whose `Pawn` is null, the next pawn of the list and applies `POSSESS(P, N)` through steps 1–4. A level's player start works without a script; a carried-over player of a `WORLD_ORIGIN_TRANSITION` world keeps its carried pawn. The input context a possessed pawn needs is the game's: a Late behaviour subscribed to `PossessionChanged` pushes it with `Input.PushContext` (`Zenith2/Docs/Design/PLAT/SimulationInput.md` → "Script-facing input API"); GAME's C++ never touches input contexts.

A pawn N destroyed while possessed by P: at the playback that marks N destroy-pending, GAME's lifecycle listener (§ "Declared access", listener interest `LIFECYCLE_EVENT_DESTROY_PENDING`) reads `N.PossessedBy` with `Zenith_WorldPlaybackAccess::ReadProperty` and, when it names a live player P whose `Pawn` is N, writes `P.Pawn = null`, `P.PossessionCount + 1` and `N.PossessedBy = null` through `Zenith_WorldPlaybackAccess::SetProperty` ([Z2-N-ARCH-544] allows property writes at playback), and records the follow-up request `NOTIFY_UNPOSSESSED(P, N)` into the buffer the listener receives. So from that sync point on no phase reads a `Pawn` naming a removed or removing entity; the follow-up reaches the queue at the next sync point and the next rules pass emits `PossessionChanged { Player = P, OldPawn = N, NewPawn = null }` on P and changes nothing else (a `NOTIFY_UNPOSSESSED` whose P no longer has a null pawn is dropped and counted `gameplay.possession.redundant`).

## The request queue

### Command kinds and payloads

Every change a script or a C++ system makes to a rule-owned gameplay property is a request: one of ten module command kinds, registered at module start through `Zenith_ModuleStartContext::RegisterCommandKind` (`Zenith2/Docs/Architecture/CompositionRoot.md` → "Module interfaces"), recordable in phases 1 to 10 (`m_uAllowedPhaseMask = 0x7FE`), each with a registered `TYPE_KIND_STRUCT` payload:

| Kind name | Payload struct (bytes) | Ops (`m_eOp`) | Fields |
|---|---|---|---|
| `Gameplay.Tags` | `Zenith_GameplayTagRequest` (24) | `ADD = 1`, `REMOVE = 2`, `REMOVE_ALL = 3` | target, tag, count (1–255) |
| `Gameplay.Attribute` | `Zenith_GameplayAttributeRequest` (32) | `DEFINE = 1`, `SET_BASE = 2`, `ADD_BASE = 3`, `SET_RANGE = 4` | target, attribute, value, min, max |
| `Gameplay.ApplyEffect` | `Zenith_GameplayApplyEffectRequest` (96) | — | target, source, `Zenith_GameplayEffectSpec` |
| `Gameplay.RemoveEffect` | `Zenith_GameplayRemoveEffectRequest` (24) | `BY_NAME = 1`, `BY_INSTANCE = 2`, `BY_GRANTED_TAG = 3` | target, name or tag, instance ID |
| `Gameplay.GrantAbility` | `Zenith_GameplayGrantAbilityRequest` (64) | — | target, `Zenith_GameplayAbilitySpec` |
| `Gameplay.Ability` | `Zenith_GameplayAbilityRequest` (32) | `ACTIVATE = 1`, `END = 2`, `CANCEL = 3`, `REVOKE = 4` | target, instigator, ability |
| `Gameplay.Timer` | `Zenith_GameplayTimerRequest` (32) | `START = 1`, `STOP = 2`, `PAUSE = 3`, `RESUME = 4` | target, name, delay, interval, repeats |
| `Gameplay.State` | `Zenith_GameplayStateRequest` (24) | `SET = 1`, `RESET = 2`, `REMOVE = 3` | target, machine, state |
| `Gameplay.Action` | `Zenith_GameplayActionRequest` (48) | `START = 1`, `COMPLETE = 2`, `FAIL = 3`, `CANCEL = 4`, `ACK = 5` | target, subject, kind, name, param, param2, value, compare, deadline ticks |
| `Gameplay.Possess` | `Zenith_GameplayPossessRequest` (24) | `POSSESS = 1`, `UNPOSSESS = 2`, `NOTIFY_UNPOSSESSED = 3` (recorded only by the module's lifecycle listener; no binding records it) | player, pawn |

Cameras need no request kind: their configuration properties have authority `CommandsOnly` and scripts change them with `SetProperty` commands (`Zenith2/Docs/Design/GAME/Cameras.md` → "Camera bindings"). Every payload begins with `uint8 m_eOp; uint8 m_auReserved[7]; Zenith_EntityHandle m_xTarget;` (16 bytes), so the queue reads the target, and with it the world tag (`Zenith_EntityHandle::GetWorldTag()`, `Zenith2/Docs/Design/ECS/WorldAPI.md` → "Zenith_EntityHandle header"), without decoding the rest. Payload structs are internal (`Internal/Zenith_GameplayRequests.h`); C++ callers inside the module record them through `Zenith_GameplayRecord<Kind>(Zenith_CommandBuffer&, const Payload&)` helpers in the same header, and scripts reach them only through the bindings.

<!-- assumption: pending MOD -->
`Zenith_ModuleStartContext::RegisterCommandKind` returns `void` (`Zenith2/Docs/Architecture/CompositionRoot.md` → "Module interfaces"), so the module cannot learn the kind ID the table assigned, which the recording helpers and the bindings need for `Zenith_CommandBuffer::Record`. GAME assumes `uint32 Zenith_WorldAttachContext::FindCommandKind(const char* szName) const` (0 when absent), answered from the sealed `Zenith_CommandKindTable::FindByName` (`Zenith2/Docs/Design/ECS/WorldAPI.md` → "Zenith_CommandKinds header"); the module caches the ten IDs at its first `AttachWorld`. Cross-area row GA5 (c).
<!-- /assumption -->

### Queue record format

```cpp
// Zenith2/Engine/ZenithGameplay/Internal/Zenith_GameplayRequestQueue.h
struct Zenith_GameplayQueuedRequest           // 128 bytes
{
	Zenith_MergeKey m_xKey;                   // 24: the record's merge key as playback passed it
	uint8 m_eKind;                           // Zenith_GameplayRequestKind (1 … 10, table order)
	uint8 m_auReserved[3];
	uint32 m_uPayloadBytes;                  // ≤ uGAMEPLAY_MAX_REQUEST_PAYLOAD_BYTES
	uint8 m_auPayload[uGAMEPLAY_MAX_REQUEST_PAYLOAD_BYTES];
};
static_assert(sizeof(Zenith_GameplayQueuedRequest) == 128);

// The world resource "Zenith.Gameplay.RequestQueue" (owning phase AINavigation): one per world, owned by the module's
// world state, storage reserved at AttachWorld from the world's heap.
struct Zenith_GameplayRequestQueue
{
	Zenith_GameplayQueuedRequest* m_axRecords = nullptr;   // capacity = gameplay.maxRequestsPerTick
	uint32 m_uCapacity = 0;
	uint32 m_uCount = 0;
	uint32 m_uDroppedSinceDrain = 0;
	uint32 m_uHighWater = 0;
};
```

The queue is a bounded queue with `QUEUE_FULL_POLICY_DROP_NEWEST` ([Z2-N-CONTRACT-343]); its capacity is the configuration key `gameplay.maxRequestsPerTick` (u32, 256 … 262,144, default 16,384, simulation-affecting), the value of the pending row need "Gameplay request capacity per tick" (`Zenith2/Docs/Design/GAME.md` → "Row needs recorded as pending"), so its memory is `capacity × 128` bytes per world (2 MiB at the default).

### Playback into the queue

The playback function of each kind (`Zenith_PfnCommandPlayback`, called on the world's sync-point node at step 7, single-threaded per world, in merge order; `Zenith2/Docs/Architecture/FramePhases.md` → "Sync points and the tick boundary"):

1. Read the target handle from the payload's first 16 bytes; a payload shorter than 16 bytes or longer than 96 is dropped (`gameplay.requests.malformed`, never an assert: [Z2-N-ARCH-490]).
2. Find the module's world state for `xWorld.GetWorldTag()` — the world whose sync point is playing the command (`Zenith2/Docs/Design/ECS/WorldAPI.md` → "Zenith_CommandKinds header" and [Z2-N-ARCH-716]: a module whose playback changes per-world module state keys that state by the playing world's tag) — in the module's tag-indexed table (`m_apxWorlds[4096]`, filled at `AttachWorld`, cleared at `DetachWorld`); a tag without state drops the request (`gameplay.requests.no_world`). A target whose `GetWorldTag()` differs from `xWorld.GetWorldTag()` — a payload naming an entity of another live world — is dropped and counted under the same `gameplay.requests.no_world`, never appended to that other world's queue: appending there from this world's sync-point node would run concurrently with that world's own playback and break the queue's one-writer rule.
3. If `m_uCount == m_uCapacity`: `m_uDroppedSinceDrain += 1`, counter `gameplay.requests.dropped`, return.
4. Copy the merge key, the kind and the payload into `m_axRecords[m_uCount++]`; update `m_uHighWater`.

The playback validates nothing else: semantic checks (stale targets, missing components, invalid specs) run at the rules system, which can emit events and which sees every request of the tick in one order. Because playback appends in merge order at sync points 1 … 10 and the rules system drains in phase 7, the drain order is: the requests of sync points 7–10 of the previous tick, then those of sync points 1–6 of this tick, each in merge order — a function of the merge keys alone.

## The rules system

### Declared access

`Zenith_GameplayRules_Register(Zenith_SystemRegistry&, Zenith_GameplayWorldState&)` adds, at `AttachWorld`:

```cpp
Zenith_SystemBuilder& xRules = xSystems.AddSystem(Zenith_SystemDesc{
	.m_szName = "Gameplay.Rules",
	.m_ePhase = PHASE_AI_NAVIGATION,
	.m_pfnRun = &Zenith_GameplayRules_Run,
	.m_pUserData = &xWorldState,
	.m_eAffinity = JOB_AFFINITY_ANY,
	.m_uCostHint = 40 });
xRules.Write(Zenith_GameplayTagsComponent::xTYPE_ID)
	.Write(Zenith_GameplayAttributesComponent::xTYPE_ID)
	.Write(Zenith_GameplayEffectsComponent::xTYPE_ID)
	.Write(Zenith_GameplayAbilitiesComponent::xTYPE_ID)
	.Write(Zenith_GameplayTimersComponent::xTYPE_ID)
	.Write(Zenith_GameplayStateMachinesComponent::xTYPE_ID)
	.Write(Zenith_GameplayActionsComponent::xTYPE_ID)
	.Write(Zenith_GameplayPlayerComponent::xTYPE_ID, axPlayerRuleProps)        // 1 Pawn, 3 PossessionCount
	.Write(Zenith_GameplayPawnComponent::xTYPE_ID, axPawnRuleProps)            // 1 PossessedBy
	.Write(Zenith_GameplayCameraDirectorComponent::xTYPE_ID, axDirectorFollowProp)   // 18 FollowPawn (step 9)
	.Read(Zenith_GameplayPlayerComponent::xTYPE_ID, axPlayerConfigProps)       // 2 CameraDirector, 4 PlayerSlot, 5 Flags (own row)
	.Read(Zenith_GameplayCameraDirectorComponent::xTYPE_ID, axDirectorPlayerProp)    // 2 Player (own row, step 9)
	.ReadAnyRow(Zenith_GameplayPawnComponent::xTYPE_ID, axPawnConfigProps)     // 2 Flags: CommandsOnly, no phase-7 writer
	.ReadAnyRow(Zenith_GameplayOwnerComponent::xTYPE_ID, axOwnerProps)         // 1 Owner, 2 Team: CommandsOnly, no phase-7 writer
	.WriteResource(Zenith_GameplayRequestQueue::xTYPE_ID)
	.ReadResource(Zenith_WorldStateResource::xTYPE_ID)                         // the continuity tick base (§ "Gameplay time")
	.EmitsCommands()
	.EmitsEvent(…) /* one call per event type of § "Event types" except Message and WorldStarted */
	.Query(xTagsQuery, xState.m_xTagsSlot)          // With Tags (write)
	.Query(xAttributesQuery, xState.m_xAttributesSlot)
	.Query(xEffectsQuery, xState.m_xEffectsSlot)    // With Effects (write), Optional Attributes (write), Optional Tags (write)
	.Query(xAbilitiesQuery, xState.m_xAbilitiesSlot)
	.Query(xTimersQuery, xState.m_xTimersSlot)
	.Query(xStatesQuery, xState.m_xStatesSlot)
	.Query(xActionsQuery, xState.m_xActionsSlot)
	.Query(xPlayersQuery, xState.m_xPlayersSlot)
	.Query(xPawnsQuery, xState.m_xPawnsSlot)
	.Query(xDirectorsQuery, xState.m_xRulesDirectorsSlot);   // With CameraDirector (FollowPawn write, Player read)
```

The two `ReadAnyRow` declarations are legal because no node of phase 7 writes those properties: both have authority `CommandsOnly` and change only at sync-point playback ([Z2-N-ARCH-485]; `Zenith2/Docs/Design/ECS/Scheduling.md` → "Authority and schedule checks" step 7). Every other column the node reads is written in phase 7 by this node or by `Gameplay.Cameras`, so `Freeze` marks it own-row-only for the node, and the node reads it only as § "Row ownership in the rules node" allows.

The module also registers its lifecycle listener (`Zenith_GameplayLifecycleListener`, interest `LIFECYCLE_EVENT_DESTROY_PENDING`, `LIFECYCLE_EVENT_CINEMATIC_START`, `LIFECYCLE_EVENT_CINEMATIC_INTERRUPT`, `LIFECYCLE_EVENT_CINEMATIC_SKIP`, `LIFECYCLE_EVENT_CINEMATIC_END`) with `Zenith_WorldAttachContext::AddLifecycleListener`. On destroy-pending of a possessed pawn it clears the possession through the playback access and records the follow-up `NOTIFY_UNPOSSESSED` (§ "Possession algorithm"); `EFFECT_FLAG_REMOVE_WITH_SOURCE` processing is left to the rules system (it checks sources each tick, § "Rules algorithm" step 4). The cinematic events drive the camera director (`Zenith2/Docs/Design/GAME/Cameras.md` → "Cinematic suspension").

### Row ownership in the rules node

ECS's own-row rule binds the node: a system reads a column it writes in the same phase for its own row only, and C++ systems are held to it by the declaration check and by `GetChunkOfEntity`, which returns null own-row-only columns for any entity other than the invocation's current one (`Zenith2/Docs/Design/ECS/Queries.md` → "Own-row and any-row reads", "Resolve and random access"). `Gameplay.Rules` is one node with no sub-jobs, and it keeps to the rule in exactly this way:

1. **One logical invocation per touched entity.** The node's logical invocation for entity E is (system order, E's simulation ID, ordinal 0). Before it reads or writes any row of E it makes E current by calling `xContext.Events(E's simulation ID)` — which also returns the event writer for E's events — and only then calls `GetChunkOfEntity(<the kind's query slot>, E, uRow)`. Re-entering an entity returns the same buffer, so all of E's events of the tick are in one buffer with sequences in processing order.
2. **Writes only to the current entity.** Every write (and every `MarkRowChanged`) is to the row of the current entity: the row `GetChunkOfEntity` returned for it, or the row being iterated in a chunk pass of steps 3–9, where the node makes each iterated entity current before touching it.
3. **Requests to other entities are follow-up records.** When processing one entity produces a change for another (possession, auto-possession, the director's follow target), the node appends a follow-up record to the node arena's worklist and applies it with its own target current (§ "Possession algorithm"; step 9 below). Facts read from one entity's row reach another only through such records or through the sorted arena lists of a multi-pass step (§ "Action evaluation"); no own-row-only column of an entity other than the current one is ever read.
4. **Cross-entity reads of columns no phase-7 node writes** use the two `ReadAnyRow` declarations above, and entity validity uses `Resolve` (the entity table, not a column).

<!-- assumption: pending ECS -->
GAME assumes that the "current simulation ID" a C++ system context tracks for `GetChunkOfEntity` is the simulation ID of the logical invocation most recently opened through `Zenith_SystemContext::Events(ulSimulationID)` or `Commands(ulSimulationID[, uInvocationOrdinal])` (ECS states that the context tracks it, not which call sets it), and that `GetChunkOfEntity` for that entity returns its own-row-only columns non-null. Cross-area row GA16.
<!-- /assumption -->

### Rules algorithm

`Zenith_GameplayRules_Run(Zenith_SystemContext& xContext)`, once per tick of the world with t its gameplay tick (§ "Gameplay time"), single node (no sub-jobs), every row access as § "Row ownership in the rules node" states:

1. **Drain dropped count.** If `m_uDroppedSinceDrain > 0`: emit `RequestsDropped { Count }` on simulation ID 0, log `gameplay: <n> requests dropped at playback (capacity <c>)`, reset it.
2. **Requests.** For each queued record in index order: decode by kind; `Resolve` the target (the entity table: it yields the simulation ID, or reports a stale, foreign or pending-destroy target); make the resolved target current (`xContext.Events(target's simulation ID)`); then `GetChunkOfEntity(<the kind's query slot>, target, row)` (null: the target lacks the component). A stale, foreign or pending-destroy target, or one without the component the kind needs, is skipped with `gameplay.requests.stale` or `gameplay.requests.missing_component` + 1 (no event, [Z2-N-ARCH-490]); otherwise apply the kind's operation to the row in place through the chunk view (`MarkRowChanged` on each written property), append its events, add the target's simulation ID to the node arena's dirty-attribute set when the operation marks attributes dirty, and apply every follow-up record the operation appended (possession) before the next queued record. Then `m_uCount = 0`.
3. **Timers.** Every row of the timers query: the firing rule of § "Timers".
4. **Effects.** Every row of the effects query: `Zenith2/Docs/Design/GAME/Abilities.md` → "Effect expiry and periods" (expiry, source-destroyed removal, periodic executions), each removal releasing its granted tags through § "Tag reference counting" and adding the row's simulation ID to the dirty-attribute set.
5. **Abilities.** Every row of the abilities query: `Zenith2/Docs/Design/GAME/Abilities.md` → "Ability end and expiry" (cooldown ends, duration expiry, cancellation by blocked tags).
6. **Actions.** The three passes of § "Action evaluation" (retention removal first).
7. **Attributes.** Every row of the attributes query that is in the dirty-attribute set (keyed by simulation ID, read from `Zenith_ChunkView::GetSimulationIDs`), or whose `Zenith.Gameplay.Attributes` row change version is newer than `xContext.GetPreviousReadVersion()` under `Zenith_IsVersionNewer` (every row when that version is 0): recompute the current values (`Zenith2/Docs/Design/GAME/Abilities.md` → "Current value evaluation") and emit `AttributeChanged` for every value whose bits changed. The version clause catches rows changed outside this node since its last run — spawned, loaded, restored, carried over or edited in the inspector; it also matches the rows this node itself committed in its previous run, which re-evaluate to the same bits and emit nothing, so the over-match costs one evaluation per row and no event.
8. **First tick.** When the tick index equals the world state's `FirstTick`: the slot-0 duplicate check (every origin) and, unless the origin is `WORLD_ORIGIN_SAVE_LOAD`, the auto-possession of § "Possession algorithm".
9. **Camera follow targets.** (a) A pass over the players query: for each player P whose `CameraDirector` (property 2) is not null, append `{ director simulation ID, P's simulation ID, P.Pawn }` to an arena list (a director handle that does not resolve is skipped and counted `gameplay.camera.lost_director`). (b) Sort the list by (director simulation ID, player simulation ID); a director named by two players keeps the lower player's entry and counts `gameplay.camera.player_mismatch`. (c) A pass over the directors query: for each director D (current), the target is its entry's pawn when D's `Player` (property 2) is null or equals that entry's player, otherwise null with `gameplay.camera.player_mismatch` + 1, and null when D has no entry; when the target differs bitwise from `D.FollowPawn`, write it and `MarkRowChanged`. `Gameplay.Cameras` runs after this node in the same phase and reads `FollowPawn` from the director's own row (`Zenith2/Docs/Design/GAME/Cameras.md` → "Declared access").

Every write is in place on the row of the current entity; the system's merge node (it declares `EmitsCommands` and events) runs ECS's default merge, which accepts each entity's buffer first-fit against the event and command capacities (`Zenith2/Docs/Design/ECS/Scheduling.md` → "Capacity acceptance") and commits the written rows at the system's merge version. An entity whose event buffer is rejected keeps its row writes (a C++ system has no rollback) and loses that tick's events of that entity, counted in `gameplay.events.rejected` with the log line `gameplay: <n> events of entity <simID> rejected at the merge (event capacity)`; the next tick proceeds normally.

Complexity: O(R + Σ rows of the ten queries × the per-row limits + P log P), R the drained requests and P the action probes plus follow entries; steps 3–9 iterate chunks in the order `GetChunk` returns and rows in chunk order, and no result depends on that order because each row's processing reads and writes that row alone, every cross-entity fact passes through a sorted arena list or a follow-up record, and events are keyed by simulation ID.

<!-- elaboration -->
A per-row sweep every tick is chosen over a priority queue of due ticks because the rows are few (gameplay entities, not every entity), the sweep needs no state of its own that a save, a replay checkpoint or a world transition would have to restore, and it has no ordering structure whose iteration could depend on insertion timing. The cost is bounded by the pending row "Gameplay framework CPU". Follow-up records are preferred over `SetProperty` commands for the other entities of a possession because a command recorded in phase 7 is played back at sync point 7, after `Gameplay.Cameras` ran, so `P.Pawn` and `N.PossessedBy` would disagree for a tick; the worklist keeps both sides consistent within one pass while every write still targets the current entity.

## The world events system

`Gameplay.WorldEvents` (phase 1, `JOB_AFFINITY_ANY`, cost hint 1) reads the resource `Zenith.Runtime.WorldState` (`Zenith2/Docs/Design/GAME/WorldHost.md` → "World state resource") and declares `EmitsEvent(Zenith.Gameplay.WorldStarted)`. When the resource's `m_ulFirstTick` equals the current tick index it emits `WorldStarted { FirstTick, WorldSeed, Origin }` on simulation ID 0, exactly once per world and for every origin (`Zenith_WorldOrigin`, `Zenith2/Docs/Design/GAME/WorldHost.md` → "World state resource"), so a Late behaviour can run world begin-play logic once per world, in the world's first tick, after the scene's `OnCreate` calls of phase 2. A world restored from a save has `FirstTick` = the restored tick + 1, so it receives the event too; a behaviour whose begin-play logic must not repeat after a load (granting starting items, for example) tests `Origin` and skips `"SaveLoad"` — GAME's own first-tick work makes the same distinction (§ "Rules algorithm" step 8). `Test_GameplayWorldEvents_StartedOnce` asserts one event per world for each origin.

## Gameplay memory accounting

The module registers one `Zenith_MemoryConsumer` (`Zenith2/Docs/Architecture.md` → "Memory governor") with subsystem `MEMORY_SUBSYSTEM_SIMULATION`, whose `GetResidentBytes` returns the sum over attached worlds of the request queue (`capacity × 128`), the camera director scratch and the world-state record; component storage itself is ECS's (Z2-B-ECS-001). Shedding: none at either level, as the table "Shedding per subsystem" states for simulation; at `MEMORY_PRESSURE_CRITICAL` the governor's `memory.simulation_over_budget` diagnostic covers it. The bytes are reported on the profiler's memory panel under `Gameplay`.

## Value type contract blocks

The listings above abbreviate the contract blocks of these value types; each block in `Zenith_GameplayComponents.h` and `Zenith_GameplayEvents.h` carries exactly the fields below (`Zenith2/Docs/Contracts.md` → "Contract blocks per type"), so `ApiContractCheck` passes on a literal transcription. Every one is `/// @contract value <Type>`; `Thread affinity` is `IMMUTABLE as a value; rows follow the authority of Framework.md "Component and resource registration"` for a component and `IMMUTABLE` for the others; `Invalidation` is `None` for all.

| Type | Ownership | Units | Errors | Version |
|---|---|---|---|---|
| `Zenith_GameplayStateMachineRecord` | stored inside `Zenith_GameplayStateMachinesComponent` | gameplay ticks for `m_ulEnteredTick`; a count for `m_uTransitionCount` | None | since Z2 Stage 3 |
| `Zenith_GameplayStateMachinesComponent` | stored as component "Zenith.Gameplay.StateMachines" | as the record | None; a fifth machine is rejected and counted (gameplay.state.full) | since Z2 Stage 3; type version 1; property IDs 1 and 2 permanent |
| `Zenith_GameplayTimerRecord` | stored inside `Zenith_GameplayTimersComponent` | gameplay ticks for `m_ulFireTick` (remaining ticks while paused); ticks for the interval; counts for repeats and fires | None | since Z2 Stage 3 |
| `Zenith_GameplayTimersComponent` | stored as component "Zenith.Gameplay.Timers" | as the record | None; a ninth timer is rejected and counted (gameplay.timers.full) | since Z2 Stage 3; type version 1; property IDs 1 and 2 permanent |
| `Zenith_GameplayActionRecord` | stored inside `Zenith_GameplayActionsComponent` | gameplay ticks for the three tick fields; the compared attribute's units for `m_fValue`; a count for `m_uRecordedActivationCount` | None | since Z2 Stage 3 |
| `Zenith_GameplayActionsComponent` | stored as component "Zenith.Gameplay.Actions" | as the record | None; a full component or a duplicate name finishes the start FAILED (gameplay.actions.full, .duplicate_name) | since Z2 Stage 3; type version 1; property IDs 1 to 3 permanent |
| `Zenith_GameplayPlayerComponent` | stored as component "Zenith.Gameplay.Player" | a count for `m_uPossessionCount`; a slot number for `m_uPlayerSlot` | None; a second slot-0 player is ignored and logged | since Z2 Stage 3; type version 1; property IDs 1 to 5 permanent |
| `Zenith_GameplayPawnComponent` | stored as component "Zenith.Gameplay.Pawn" | None | None | since Z2 Stage 3; type version 1; property IDs 1 and 2 permanent |
| `Zenith_GameplayOwnerComponent` | stored as component "Zenith.Gameplay.Owner" | a game-defined team number | None; a chain longer than uGAMEPLAY_MAX_OWNER_HOPS resolves to null | since Z2 Stage 3; type version 1; property IDs 1 and 2 permanent |
| each `Zenith_Gameplay…Event` struct of § "Event types" | plain value: one event record, owned by ECS's event arena between emission and delivery | as each member's comment; a tick index for `m_ulFirstTick`; `m_e…` members hold the enumeration named in the event table | None | since Z2 Stage 3 (the attribute and effect events too: registered by the API-only slice, emitted since Stage 4); property IDs follow member order and are permanent |
