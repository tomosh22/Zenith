# DevilsPlayground/Source

Cross-cutting state + algorithms. Anything in this folder is a shared
"library" that the per-entity behaviours in `Components/` lean on. The
discipline is: behaviours read and write game state **only** through
the namespaces declared here — they don't reach into each other's
headers, and they don't carry their own state buckets.

## File map

```
PublicInterfaces.{h,cpp}      # DP_Player / DP_Items / DP_Interactables / DP_AI / DP_Fog /
                              #   DP_Win / DP_Night / DP_Query / DP_Save -- the contract every
                              #   behaviour talks through. State lives in PublicInterfaces.cpp's
                              #   anonymous namespace.
DevilsPlayground_Tags.h       # DP_ItemTag enum + helpers (Iron / Key / Objective1-5 / SkeletonKey /
                              #   Spike). Used by DPItemBase + the side-tables in DP_Items.
DPInputActions.h              # WASD / Q-E / F / Space / Esc / Shift / Ctrl / G / click readers.
                              #   Routed through Zenith_Input so the simulator can drive them.
DPFogPass.{h,cpp}             # Engine fog override + post-fog hook registration. DP disables the
                              #   engine's 6 fog passes and registers a custom one that reads
                              #   DP_Fog's hole list each frame.
DPMaterials.{h,cpp}           # Tinted-cube material side-table. Maps DP_ItemTag -> debug colour;
                              #   `GetOrCreateColouredVariant` lazily clones a base material with
                              #   a tint override. Kept post-2026-05-19 as the placeholder visual
                              #   language until S2 art arrives.

DPTelemetry.{h,cpp}           # Binary telemetry recorder hooks. RAII `Hooks` struct subscribes to
                              #   9 DP event types + the per-frame entity sampler and routes them
                              #   into the engine's Zenith_Telemetry recorder. v3 adds AI intent,
                              #   life timer, held item, camera, per-frame perf, and 12 event
                              #   types (Apprehend lifecycle, PerceptionContactBegin/End, etc).
DPTelemetryAnalyzer.{h,cpp}   # Verdict library reading a .ztlm + applying pass/fail criteria.
                              #   14-criterion stable enum; per-criterion reason strings.

DP_Archetypes.{h,cpp}         # Loads Config/Archetypes.json; exposes `Get(id)` -> const ref to
                              #   archetype data (life timer, abilities, tint). Filter "mvp": true
                              #   gives the 4 MVP archetypes (Farmhand / Beggar / Devout / Child).
DP_Reagents.{h,cpp}           # Loads Config/Reagents.json; exposes per-tag pickup channel +
                              #   special-behaviour metadata (BogWater 8 s evaporate, BellSoul
                              #   ring-on-pickup, etc).
DP_Save.{h,cpp}               # DP_RunState struct + serialisation. Schema version anchored;
                              #   corruption + version mismatch fall back to default. See
                              #   Docs/SaveFormat.md.
DP_Json.{h,cpp}               # Shared JSON parser. Extracted 2026-05-22 from the four byte-identical
                              #   copies that lived in DPMaterials / DP_Tuning / DP_Archetypes /
                              #   DP_Reagents. `DP_Json::JsonValue` + `DP_Json::LoadJsonFile` are
                              #   the only public symbols; the parser implementation is anon-
                              #   namespace-local to DP_Json.cpp.
DP_Tuning.{h,cpp}             # Loads Config/Tuning.json at startup, flattens dotted-key tree into
                              #   a Zenith_Vector<KVPair>. `DP_Tuning::Get<T>(key)` is the single
                              #   numerical-value lookup; every gameplay constant routes through
                              #   it.

DPUI.h                        # UI helpers (text formatting, anchor + size utilities).

DPProcLevel/                  # Procgen level generator. See below.
```

## DP_* namespaces (the contract)

`PublicInterfaces.h` declares the entire game-side state surface as a
flat set of namespaces. The pattern is intentional: behaviours that
need to talk to each other go through the namespace, **never** through
a sibling behaviour's header.

| Namespace | Role | Notable functions |
|---|---|---|
| `DP_Player` | Possession + held-item table | `GetPossessedVillager` / `SetPossessedVillager` / `TryVoluntaryPossessSwitch` (cooldown + range gates) / `GetHeldItemTag` / `SetHeldItem` / `RemoveHeldItem` (early-returns on no-held, SourceBugFixed) / scent table / `ResetForNewRun` (canonical reset; clears per-run state). Anchor position tracking for possession-range checks. |
| `DP_Items` | Side-table EntityID → DP_ItemTag | `GetItemTag(id)` / `FindItemByTag(tag)` (returns INVALID_ENTITY_ID on miss; SourceBugFixed) / `GetItemWorldPos(id)`. Populated by DPItemBase OnAwake; depopulated OnDestroy. |
| `DP_Interactables` | Marker for non-behaviour interactables | Currently a no-op stub. Reserved for proximity-only world entities. |
| `DP_AI` | Priest blackboard bridge + noise emission | `EmitNoise(pos, loudness, radius)` → wraps `Zenith_PerceptionSystem::EmitSoundStimulus`. `NotifyAllPriestsOfInvestigatePos(vec3)` is the direct-BB fanout for map-wide stimuli (BellSoul) that bypasses the perception-clamp. Defines BB key constants (`BB_KEY_TARGET_WITH_DEVIL`, `BB_KEY_HAS_INVESTIGATE_POS`, etc). |
| `DP_Fog` | Fog-of-war hole registry | `ClearAllFogHoles()` + `RegisterFogHole(pos, radius)`. DPFogPass_Behaviour drives this every frame. |
| `DP_Win` | Victory state | `GetCollectedObjectivesMask()` (bitmask of 5 objectives) / `DepositObjective(tag)` / `HasWon()` / `DP_OnVictory` event dispatched when `popcount(mask) >= night.reagents_required_for_victory` (default 3-of-5 since 2026-05-22; was `mask == 0b11111` strict-5-of-5 before). The win threshold is a single tuning knob -- bump it to 5 to restore the all-of-5 design. |
| `DP_Night` | Dawn timer | `StartNight(duration)` / `TickNight(dt)` / `GetNightTimeRemaining()` / `IsNightActive()`. Fires `DP_OnRunLost{Dawn}` exactly once on cross-zero. |
| `DP_Query` | Template helpers | `ForEachScriptInActiveScene<T>(lambda)` — the way to iterate scripts by type (scripts live INSIDE Zenith_ScriptComponent so direct `Query<T>` doesn't work). |
| `DP_Save` | Run state serialisation | `DP_RunState` struct + binary round-trip via Zenith_SaveData. Schema versioned (`uSchemaVersion = 1`); future versions migrate or fall back to default. |

The state buckets (held-item table, scent table, fog-hole list, win
mask, etc) live in the .cpp's anonymous namespace — accessed only via
the namespace functions, never directly from outside.

## DPProcLevel — procgen level generator

The single gameplay scene (`ProcLevel`, build index 1) is built at
runtime by `DPProcLevel::Generate(seed, cfg, ...)` rather than loaded
from a hand-authored `.zscen`. The generator is **bit-deterministic**
across `/fp:fast` Debug + Release builds — verified by
`Test_ProcLevel_DeterminismCheck`. Every shape-determining decision
runs in integer math at millimetre precision; floats only appear at
the public layout boundary, with the conversion pinned to
`c * 0.001f` so the optimiser cannot substitute a divide.

Files:

- `DPProcLevel_Generator.{h,cpp}` — the pipeline: BSP → rooms → doors
  → walls → game-element placement → AI placement. See the giant block
  comment at the top of the .cpp for the full integer-coord design.
- `DPProcLevel_LevelLayout.h` — public `LevelLayout` struct (floats at
  the boundary, integer math internal). Returned by Generate.
- `DPProcLevel_JsonExport.{h,cpp}` — layout → JSON for the offline
  visualiser (`Tools/dp_proclevel_visualise.py`).

Layout fields the bootstrap (`DPProcLevelBootstrap_Behaviour`)
consumes:

- `rooms[]` (rectangular footprints, possibly non-square),
- `walls[]` (line segments with door gaps already cut),
- `gameElements[]` (typed entries: forge, door, chest, noise machine,
  pentagram, iron spawner, objective spawners),
- `villagerSpawns[]` (positions + archetype id),
- `priestSpawn` + `priestPatrolNodes[]`.

The bootstrap spawns one entity per element type using existing
`Components/` behaviours; nothing about the entity layer knows about
procgen. Switching to hand-authored levels would only require a
different `Bootstrap` that calls `AddStep_*` patterns instead.

Seed is overridable via `DP_PROCGEN_SEED` env var (PR #121) — used by
the seed-matrix runner to cycle through multiple layouts.

## Telemetry recorder + analyser

The recorder is a binary stream (`.ztlm`) + a sidecar JSON. Engine
piece lives in `Zenith/Telemetry/`; DP-specific subscription lives
here.

`DPTelemetry::Hooks` is a RAII helper: construct → subscribes to the 9
DP event types via `Zenith_EventDispatcher`; destruct → unsubscribes.
The recorder samples the world every `samplePeriodFrames` (default 6 =
10 Hz at 60 Hz fixed-dt) and emits an `EntitySnapshot` per visible
entity. v3 added `uHeldItemTag`, `fLifeRemaining`, `aiIntent` (priest
BT branch), camera state, and per-frame `frameMs`.

Event types (12 active as of v3):

```
PossessionChanged          Re-possession or first possession
VillagerDied               Life timer expired or apprehend complete
ObjectivePlaced            Pentagram delivery
ItemPickup                 Auto-proximity pickup
Interact                   F-press inside an interactable's range
InteractionEnd             Range or channel-end
ChestOpened
DoorOpened
ForgeCrafted
PauseToggle
ApprehendChannelStart      Priest entered apprehend range + began channel
ApprehendChannelComplete   Channel finished -> DP_OnRunLost{Apprehended}
ApprehendChannelInterrupted Channel broken (SwitchedTarget / TargetLost / OutOfRange)
PerceptionContactBegin     Priest's awareness of target crossed 0.4
PerceptionContactEnd       Awareness fell back below 0.4
RunLost                    Aggregate event with cause enum
```

The analyser (`DPTelemetryAnalyzer`) reads a `.ztlm` and applies the
14-criterion verdict library — `VictoryFired`, `AnySprintFrame`,
`AnyWalkQuietFrame`, `PriestInvestigatedAtLeastOnce`, etc. Each
criterion returns `Pass | Fail | NotApplicable` + a reason string.

## DP_Tuning

The Tuning.json design: every numerical gameplay value (life timer,
sprint cost, priest perception ranges, fog memory durations, etc) is
keyed by a dotted string and read via `DP_Tuning::Get<T>(key)`. Hot
keys are read once at OnAwake and cached on the behaviour; cold keys
can be read every frame.

Hierarchy: `possession.life_timer_default_s`, `movement.sprint_speed_mps`,
`priest.sight_range_m`, `interactables.forge_audible_loudness`, etc.

`_comment_*` keys are documentation only and ignored by the loader.
Use them generously — they're the canonical record of why a value is
what it is.

## Source-bug guards

Three guarded ports from the UE5 source map (each marked
`SourceBugFixed:` in code):

1. **`DPInteractable_Behaviour::OnExitRange`** (Components/) — source
   removed the wrong delegate type. Port stores the exact subscription
   handle on rising-edge and unsubscribes that exact handle on
   falling-edge plus in OnDisable/OnDestroy.
2. **`DP_Player::RemoveHeldItem`** — source null-derefs when no held
   item. Port early-returns; verified by `DP_HeldItem_Test`.
3. **`DP_Items::FindItemByTag`** — source derefs on miss. Port returns
   `INVALID_ENTITY_ID`; verified by `DP_FindItemByTag_Test`.

## Adding a new namespace

If a new behaviour needs cross-cutting state, the right pattern is:

1. Declare a new `DP_Foo` namespace in `PublicInterfaces.h`.
2. Put state in `PublicInterfaces.cpp`'s anonymous namespace.
3. Expose getters/setters/lifecycle hooks from the namespace.
4. Add a between-tests reset in `DevilsPlayground.cpp`'s
   `Project_RegisterBetweenTestsHook` so batched tests don't leak
   state across test boundaries.
5. Hand-write a couple of tests in `Tests/Test_DP_Foo.cpp` that pin
   the contract.

Behaviours should never reach into each other's headers; the
public-interface contract is what keeps the dependency graph flat.
