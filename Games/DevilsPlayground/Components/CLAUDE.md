# DevilsPlayground/Components

Per-entity behaviour scripts. Every gameplay actor in the scene
(villager / priest / door / chest / pentagram / etc.) is a script that
attaches to a `Zenith_ScriptComponent` and overrides the lifecycle
hooks (`OnAwake` / `OnStart` / `OnUpdate` / `OnDisable` / `OnDestroy`).

Cross-entity coordination happens through the `DP_*` namespaces in
`Source/PublicInterfaces.h` — behaviours **never** reach into each
other's headers.

## File map

### Player-side behaviours

```
DPPlayerController_Behaviour.h    # Click-to-possess raycast. Reads mouse position via
                                  #   Zenith_Input::GetMousePosition (so the simulator can
                                  #   drive clicks deterministically) + builds a world ray +
                                  #   picks the closest villager hit. Calls
                                  #   DP_Player::TryVoluntaryPossessSwitch (cooldown + range
                                  #   gates) for player clicks; SetPossessedVillager for
                                  #   system paths (tests, death-respawn).
DPVillager_Behaviour.h            # Possessable villager. Holds the per-archetype life timer
                                  #   (Farmhand 45 s / Devout 45 / Beggar 37.5 / Child 22.5
                                  #   as of 2026-05-22) + WASD movement (camera-relative;
                                  #   matches DPInputActions). States: Idle / Possessed /
                                  #   Fainted / Dead. Sprint (Shift) drains life at +1.5 s/s
                                  #   over base (was 1.0 -- bumped 2026-05-22). Walk-quiet
                                  #   (Ctrl) uses 0.875x jog speed + 0.25x footstep loudness.
DPOrbitCamera_Behaviour.h         # Orbit camera over the possessed villager. Q/E yaw + mouse-
                                  #   wheel zoom (EXT-4). DPVillager position drives orbit
                                  #   target. Not a Zenith_CameraComponent (which is FPS-only);
                                  #   own implementation of the orbit math.
DPHUDController_Behaviour.h       # The HUD. Life bar (colour gradient) + held-item readout +
                                  #   objective counter + per-state banner. Subscribes to
                                  #   DP_OnVictory + DP_OnRunLost (per-cause copy: "CAUGHT BY
                                  #   AELFRIC" / "DAWN BREAKS" / "NO VESSELS REMAIN"). Also
                                  #   houses ControlsHint / TutorialHint / HelpOverlay /
                                  #   MenuHowTo (the instructional HUD).
DPMainMenuController_Behaviour.h  # Front-end Play button -> LoadSceneByIndex(1) = ProcLevel.
                                  #   Lives in scene 0 (FrontEnd).
DPPauseMenuController_Behaviour.h # Esc-toggle pause overlay. Migrates to persistent scene
                                  #   (singleton pattern) on first OnStart so it can pump
                                  #   input while the gameplay scene is paused -- otherwise
                                  #   the player couldn't unpause. R-key restart / Q-key
                                  #   quit-to-FrontEnd routed here.
```

### Interactable behaviours

`DPInteractable_Behaviour.h` is the base for all proximity + F-press
interactables. It subscribes to OnEnterRange / OnExitRange events and
fires `DP_OnInteract` on rising-edge F-press inside range.

```
DPInteractable_Behaviour.h        # Base class. Distance-based proximity (default 2 m radius
                                  #   from Tuning.json). F-press fires DP_OnInteract. Stores
                                  #   the EXACT subscription handle on rising-edge so the
                                  #   falling-edge unsubscribe works (SourceBugFixed against
                                  #   the UE5 source map's wrong-delegate-type bug).
DPDoor_Behaviour.h                #   Lerp-rotate door. Key-gated; consumes the held Key on
                                  #   interact. Triggers a path-grid invalidation via
                                  #   OnInteractEvent so the test-bot pathing can re-route.
                                  #   Collider geometry rebalance 2026-05-22: was a default
                                  #   (1,1,1) cube at y=0 (1m tall, in floor) -- bot raycasts
                                  #   treated it as floor and walked through "locked" doors.
                                  #   Now spawned with scale (0.3, 4.0, 2.0) at y=1, rotated
                                  #   to align with the wall axis (procgen stores fYawRadians
                                  #   on the door GameElement). OnAwake captures the entity
                                  #   transform yaw as m_fClosedYaw so the open-rotation
                                  #   interpolation starts from the procgen-set angle.
                                  #   SetIncludeInNavMesh(false) so the navmesh emits walkable
                                  #   polygons through the doorway; SyncNavMeshBlock toggles
                                  #   the BLOCKED flag at runtime for priest pathing.
DPDoubleDoor_Behaviour.h          #   Two-leaf door. FindChildTransform("Leaf_L" / "Leaf_R")
                                  #   drives per-leaf rotation. Same key-gated semantics.
DPChest_Behaviour.h               #   Lid pivot + (WIP) loot dispense. Currently only the lid
                                  #   pivot is wired; loot drop is post-MVP.
DPForge_Behaviour.h               #   Recipe Iron -> Key. Per-instance recipes (Iron+Wood ->
                                  #   Spike, Iron+Brass -> SkeletonKey, etc, post-MVP). On
                                  #   interact: consume held item + spawn output entity +
                                  #   auto-equip to villager.
DPPentagram_Behaviour.h           #   Win condition. On interact: if held item tag is in
                                  #   Objective1..5 and not already collected, mark the bit
                                  #   in DP_Win's mask. 5/5 dispatches DP_OnVictory.
DummyNoiseMachine_Behaviour.h     #   Deliberate hearing stimulus. F-press emits a
                                  #   loudness=1.0 / radius=19 m sound via DP_AI::EmitNoise.
                                  #   The one in-game source of deliberate priest aggro --
                                  #   Stealth personality opts out (bRunNoiseMachine=false).
```

### Item behaviours

```
DPItemBase_Behaviour.h            # Base class for tagged pickups (Iron, Key, Objective1-5,
                                  #   SkeletonKey, Spike). OnAwake registers
                                  #   EntityID -> DP_ItemTag in DP_Items's side-table;
                                  #   OnDestroy depopulates. OnUpdate handles auto-proximity
                                  #   pickup at 1.5 m (DPItemBase reads the closest villager
                                  #   without an item and auto-picks-up). Tinted-cube
                                  #   visualisation via DPMaterials.
DPItemSpawn_Behaviour.h           # Spawn anchor. Placed by procgen; no auto-spawn -- the
                                  #   DPItemManager triggers the actual instantiation.
DPItemManager_Behaviour.h         # Singleton. OnStart walks every DPItemSpawn in the scene
                                  #   and instantiates the configured item entity at each.
                                  #   Procgen populates the spawners' configured-item-tag.
```

### AI behaviours

```
Priest_Behaviour.h                # Roaming witch-finder Aelfric. Owns a Zenith_BehaviorTree;
                                  #   manual Tick. BridgePerceptionToBlackboard runs each
                                  #   frame to translate Zenith_PerceptionSystem targets into
                                  #   BB keys (BB_KEY_TARGET_WITH_DEVIL,
                                  #   BB_KEY_HAS_INVESTIGATE_POS, BB_KEY_PATROL_TARGET,
                                  #   BB_KEY_HIGH_SCENT_TARGET).
DP_BT_Nodes.h                     # 4 custom BT nodes:
                                  #   DP_BTAction_FindPos       -- pick a random reachable
                                  #                                navmesh point for patrol
                                  #   DP_BTAction_HasInvestigate -- check the investigate-pos
                                  #                                BB key
                                  #   DP_BTAction_Apprehend      -- channel for
                                  #                                priest.apprehend_channel_s
                                  #                                while within range; dispatch
                                  #                                DP_OnRunLost{Apprehended}
                                  #                                on complete. Channel
                                  #                                breaks on switch /
                                  #                                target-lost / out-of-
                                  #                                range.
                                  #   DP_BTAction_PursueWithChannel -- pursue path + tighten
                                  #                                  on approach.
```

### Visual / system behaviours

```
DPFogPass_Behaviour.h             # Per-frame fog-hole rebuild. ClearAllFogHoles at start of
                                  #   frame; RegisterFogHole per possessed-villager (8 m
                                  #   radius), per-unpossessed-villager (1.5 m, line-of-sight
                                  #   only -- "memory" of where you saw them), per scene
                                  #   light (radius from light.range).
DPProcLevelBootstrap_Behaviour.h  # ProcLevel scene's bootstrap script. OnAwake calls
                                  #   DPProcLevel::Generate(seed, cfg) + spawns one entity
                                  #   per layout element (room walls, doors, chests, forge,
                                  #   pentagram, item spawners, villagers, priest, priest
                                  #   patrol nodes). Seed from DP_PROCGEN_SEED env var if
                                  #   set, else the default seed (currently 0xCAFEBEEF).
                                  #
                                  #   2026-05-22 changes:
                                  #     * Door entities scaled (0.3, 4.0, 2.0) at y=1 and
                                  #       rotated to align with wall axis (procgen now
                                  #       stores fYawRadians on the door GameElement).
                                  #       Doors actually block bot raycasts + player capsule
                                  #       physics now -- prior to this, closed doors only
                                  #       blocked navmesh AI (priest).
                                  #     * Item / pentagram / chest colliders now navmesh-
                                  #       excluded (SetIncludeInNavMesh(false)). The bot
                                  #       paths TO these entities for pickup / delivery;
                                  #       their colliders would otherwise carve holes in
                                  #       the navmesh and break FindPath queries.
                                  #       Pickup remains proximity-based (1.5 m), not
                                  #       collision-based.
```

## The behaviour pattern

Every script follows the same skeleton:

```cpp
#pragma once
#include "EntityComponent/Zenith_ScriptBehaviour.h"

class DPFoo_Behaviour ZENITH_FINAL : Zenith_ScriptBehaviour
{
    friend class Zenith_ScriptComponent;
public:
    ZENITH_BEHAVIOUR_TYPE_NAME(DPFoo_Behaviour)

    DPFoo_Behaviour(Zenith_Entity& /*xParentEntity*/) {}
    // ^ Constructor doesn't forward to Zenith_ScriptBehaviour(xParent) -- the
    //   base has no such constructor. m_xParentEntity is assigned by
    //   Zenith_ScriptComponent after CreateInstance returns.

    virtual void OnAwake() override   { /* register side-tables, cache cross-refs */ }
    virtual void OnStart() override   { /* subscribe to events, kick state machines */ }
    virtual void OnUpdate(float fDt) override { /* per-frame logic */ }
    virtual void OnDisable() override { /* unsubscribe -- CRITICAL for DPInteractable */ }
    virtual void OnDestroy() override { /* deregister side-tables */ }
};
```

**Note on the inheritance syntax:** `ZENITH_FINAL` leaf behaviours use
*private* inheritance (`: Zenith_ScriptBehaviour`, no `public`) — this
matches every other shipping game project in the engine
(Sokoban / Marble / Combat / TilePuzzle / AIShowcase / etc.). The
`friend class Zenith_ScriptComponent;` declaration is what grants the
script-component access to the lifecycle hooks; private inheritance
still permits the derived class to override the virtual functions for
polymorphic dispatch.

The only place we use `public Zenith_ScriptBehaviour` is when the
behaviour is *itself* a base for further derivation — see
[DPInteractable_Behaviour.h](DPInteractable_Behaviour.h), which is
the parent of DPDoor / DPChest / DPForge / DPPentagram / DPDoubleDoor /
DummyNoiseMachine. Subclasses then use
`: public DPInteractable_Behaviour` to inherit the proximity +
F-press wiring.

The `ZENITH_BEHAVIOUR_TYPE_NAME(DPFoo_Behaviour)` macro registers the
C++ factory at static-init via `Zenith_ScriptAsset::RegisterFactory`.
The corresponding `.zscript` asset file is written automatically on
the next tools-build run by
`Zenith_ScriptAsset::SyncRegisteredTypesToDisk`.

### Static-init dead-strip

MSVC linkers dead-strip object files that aren't referenced from any
`.cpp`. The `ZENITH_BEHAVIOUR_TYPE_NAME` static-init registrar only
fires if the .obj is pulled into the link, so every behaviour header
must be `#include`d from `DevilsPlayground.cpp`. If a new behaviour
doesn't appear in the scripts list at runtime, that's the first thing
to check.

### Cross-behaviour communication

A behaviour can read another behaviour's outputs via:

- **The DP_* namespaces** (`DP_Player`, `DP_Items`, `DP_Win`, etc) —
  preferred.
- **Events via Zenith_EventDispatcher** — `DP_OnInteract`,
  `DP_OnVictory`, `DP_OnRunLost`, `DP_OnVillagerDied`, `DP_OnBellRing`,
  `DP_OnInteractionEnd`, etc.
- **`DP_Query::ForEachScriptInActiveScene<T>(lambda)`** — find every
  script of type T in the active scene. Linear scan; cache the result.

Behaviours do **NOT** include each other's headers. That's the
contract.

**Documented exceptions (creation/lifecycle, not cross-state reads).** A few
behaviours legitimately include sibling behaviour headers because they
*create* / attach them — something a flat `DP_*` forwarder can't express
cleanly: `DPProcLevelBootstrap_Behaviour` (the scene's master spawn-factory —
instantiates one entity per procgen layout element), `DPItemManager_Behaviour`
(iterates its own `DPItemSpawn` anchors and instantiates `DPItemBase` items),
and `DPForge_Behaviour` (spawns an item entity and attaches `DPItemBase`). Each
such include is marked `// Contract exception` in code (grep for it). Every *read* cross-query goes through a `DP_*` forwarder
instead (e.g. `DP_Win::IsPentagramInRange`, `DP_Player::IsBeggarVillager`,
`DP_AI::GetNearestPriestDistanceFrom`, `DP_Interactables::FindNearestInteractableType`).

### Subscription cleanup discipline

If a behaviour subscribes to an event via a captured lambda
(`Subscribe<DP_OnFoo>([this](const DP_OnFoo&){ ... })`), it MUST
unsubscribe in OnDisable + OnDestroy. The lambda captures `this`; if
the entity is destroyed mid-range, the captured pointer dangles and
the dispatcher crashes on the next dispatch. `DPInteractable` is the
canonical example — it stores the exact subscription handle on
rising-edge and unsubscribes that exact handle.

## Adding a new behaviour

1. Drop a header under `Components/` matching the existing pattern.
2. Add `ZENITH_BEHAVIOUR_TYPE_NAME(DPFoo_Behaviour)` inside the class body.
3. Constructor: `DPFoo_Behaviour(Zenith_Entity&) {}` — do NOT forward
   to base.
4. `#include "Components/DPFoo_Behaviour.h"` from
   `DevilsPlayground.cpp` so the static-init registrar pulls in.
5. Run Sharpmake (`cmd /c '.\Sharpmake_Build.bat < nul'` from `Build/`)
   to regenerate the vcxproj with the new file.
6. Reference from authoring:
   - Hand-authored scenes: `AddStep_AttachScript("DPFoo_Behaviour")`
     inside `Project_RegisterEditorAutomationSteps`.
   - Procgen scenes: have `DPProcLevelBootstrap` spawn it at runtime
     from a new procgen layout element type.

## Known gotchas

- **DPInteractable subscribers must clean up.** The OnExitRange
  unsubscribe path is the most common source of dangling-lambda
  crashes if you forget.
- **`Zenith_ScriptComponent::GetScript<T>()` is a linear scan by
  type-name string.** Cheap, but cache the pointer once after OnStart
  if you call it more than ~once per frame.
- **Procgen geometry is not tuned for hand-tuned priest tests.** Tests
  that need a specific priest/villager spatial arrangement may fail
  on procgen because the navmesh agent steers the priest back to its
  nearest patrol-node polygon. Stage such tests by spawning fresh
  entities at known clear positions rather than teleporting
  procgen-spawned entities. (See Tests/CLAUDE.md.)
- **`Project_RegisterEditorAutomationSteps` only runs in tools
  builds.** Wrapped in `#ifdef ZENITH_TOOLS`. Non-tools builds load
  pre-saved `.zscen` files; re-run the tools build after changing
  scene authoring or behaviours.
