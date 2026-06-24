# DevilsPlayground/Components

Per-entity game components. Every gameplay actor in the scene
(villager / priest / door / chest / pentagram / etc.) is a game ECS
component registered with the component-meta registry; the registry
concept-detects and dispatches the lifecycle hooks
(`OnAwake` / `OnStart` / `OnUpdate` / `OnDisable` / `OnDestroy`).

Cross-entity coordination happens through the `DP_*` namespaces in
`Source/PublicInterfaces.h` — components **never** reach into each
other's headers.

## File map

### Player-side components

```
DPPlayerController_Component.h    # Click-to-possess raycast. Reads mouse position via
                                  #   Zenith_Input::GetMousePosition (so the simulator can
                                  #   drive clicks deterministically) + builds a world ray +
                                  #   picks the closest villager hit. Calls
                                  #   DP_Player::TryVoluntaryPossessSwitch (cooldown + range
                                  #   gates) for player clicks; SetPossessedVillager for
                                  #   system paths (tests, death-respawn). Also OWNS the
                                  #   per-run state block (held items, scent, possession,
                                  #   win mask, night timer) that the DP_Player / DP_Win /
                                  #   DP_Night namespace functions reach via Instance().
DPVillager_Component.h            # Possessable villager. Holds the per-archetype life timer
                                  #   (Farmhand 45 s / Devout 45 / Beggar 37.5 / Child 22.5
                                  #   as of 2026-05-22) + WASD movement (camera-relative;
                                  #   matches DPInputActions). States: Idle / Possessed /
                                  #   Fainted / Dead. Sprint (Shift) drains life at +1.75 s/s
                                  #   over base. Walk-quiet (Ctrl) uses 0.875x jog speed +
                                  #   0.25x footstep loudness.
DPOrbitCamera_Component.h         # Orbit camera over the map centre (pinned at ~50, 0, 50; does
                                  #   NOT follow the possessed villager). Q/E yaw + mouse-wheel
                                  #   zoom (EXT-4). Not a Zenith_CameraComponent (which is
                                  #   FPS-only); own implementation of the orbit math. Gym
                                  #   scenes can override the target via SetOrbitTarget().
DPHUDController_Component.h       # The HUD. Life bar (colour gradient) + held-item readout +
                                  #   objective counter + per-state banner. Subscribes to
                                  #   DP_OnVictory + DP_OnRunLost (per-cause copy: "CAUGHT BY
                                  #   AELFRIC" / "DAWN BREAKS" / "NO VESSELS REMAIN"). Also
                                  #   houses ControlsHint / TutorialHint / HelpOverlay /
                                  #   MenuHowTo (the instructional HUD).
DPMenuRelay_Component.h           # Menu-button shim. Wires the FrontEnd UI buttons to
                                  #   "MenuPlay"/"MenuQuit" custom events consumed by
                                  #   DP_MainMenu.bgraph (Play -> LoadSceneByIndex node;
                                  #   Quit -> DPRequestQuit node). Lives in scene 0.
DPPauseMenuController_Component.h # Esc-toggle pause overlay. Migrates to persistent scene
                                  #   (singleton pattern) on first OnStart so it can pump
                                  #   input while the gameplay scene is paused -- otherwise
                                  #   the player couldn't unpause. R-key restart / Q-key
                                  #   quit-to-FrontEnd routed here. NOTE: the cross-scene
                                  #   move RELOCATES the component (move-construct); its
                                  #   hand-written moves keep the singleton + event
                                  #   subscriptions valid, and DontDestroyOnLoad is the
                                  #   final statement of OnStart by contract.
```

### Interactable components

`DPInteractable_Base.h` is the plain (NON-component, NON-registered)
C++ base class for all proximity + F-press interactables. It subscribes
to OnEnterRange / OnExitRange events and fires `DP_OnInteract` on
rising-edge F-press inside range. Only the concrete leaves register as
components; they publicly inherit the base, and the leaf constructor
passes the parent entity through to the base's protected constructor.

Most interactable LOGIC now lives in boot-authored Behaviour Graphs (see
"Behaviour Graphs & shims" below); the C++ leaves that remain are input
plumbing + systems shims.

```
DPInteractable_Base.h             # Base class. Distance-based proximity (default 2 m radius
                                  #   from Tuning.json). F-press fires DP_OnInteract. Stores
                                  #   the EXACT subscription handle on rising-edge so the
                                  #   falling-edge unsubscribe works (SourceBugFixed against
                                  #   the UE5 source map's wrong-delegate-type bug). Hand-
                                  #   written moves re-subscribe the this-capturing lambda
                                  #   after a component-pool relocation.
DPGraphInteractable_Component.h   #   Generic graph-interact shim leaf: on F-press, fires the
                                  #   "Interact" custom event into the entity's GraphComponent
                                  #   with the interacting villager as a packed-EntityID
                                  #   payload. Hosts pentagram / chest / noise machine (their
                                  #   logic lives in DP_Pentagram / DP_Chest /
                                  #   DP_NoiseMachine .bgraph).
DPDoor_Component.h                #   SYSTEMS SHIM for the graph-driven single-leaf door.
                                  #   Keeps in C++: navmesh portal stitch (OnStart) + runtime
                                  #   BLOCKED toggling, collider SOLID-iff-Closed sensor
                                  #   toggle, lock tint, closed-yaw capture + swing rotation,
                                  #   logical-centre anchor, F-press rising-edge latch.
                                  #   Decisions live in DP_Door.bgraph; state on its
                                  #   blackboard (anim 0-3 / openT / requiredKey) — the
                                  #   accessors (IsOpen/GetAnim/BlocksPath/GetRequiredKey)
                                  #   read the blackboard, so consumers (bot grid, priest's
                                  #   TryInteract, HUD scan) compile unchanged. Graph nodes
                                  #   call OnDoorStateChanged()/ApplyRotationFromT()/
                                  #   RefreshLockTint() back on the shim SYNCHRONOUSLY (no
                                  #   1-frame navmesh race). Bootstrap MUST attach the graph
                                  #   before SetRequiredKey (it writes the blackboard).
DPForge_Component.h               #   Recipe Iron -> Key. Per-instance recipes via SetRecipe.
                                  #   On interact: consume held item + spawn output entity +
                                  #   auto-equip to villager. (Still pure C++.)
```

### Item components

```
DPItemBase_Component.h            # Tagged pickup (Iron, Key, Objective1-5, SkeletonKey,
                                  #   Spike, reagents). OnAwake registers
                                  #   EntityID -> DP_ItemTag in DP_Items's side-table;
                                  #   OnDestroy depopulates. OnUpdate handles auto-proximity
                                  #   pickup at 1.5 m. Tinted-cube visualisation via
                                  #   DPMaterials.
DPItemSpawn_Component.h           # Spawn anchor. Placed by procgen / tests; no auto-spawn --
                                  #   the DPItemManager triggers the actual instantiation.
DPItemManager_Component.h         # Singleton. OnStart walks every DPItemSpawn in the scene
                                  #   and instantiates the configured item entity at each.
                                  #   Owns the EntityID -> tag side-table behind DP_Items.
```

### AI components

```
Priest_Component.h                # Roaming witch-finder Aelfric. Owns a Zenith_BehaviorTree;
                                  #   manual Tick. BridgePerceptionToBlackboard runs each
                                  #   frame to translate Zenith_PerceptionSystem targets into
                                  #   BB keys (BB_KEY_TARGET_WITH_DEVIL,
                                  #   BB_KEY_HAS_INVESTIGATE_POS, BB_KEY_PATROL_TARGET,
                                  #   BB_KEY_HIGH_SCENT_TARGET). Hand-written moves re-wire
                                  #   the AIAgentComponent's pointer to the by-value
                                  #   Zenith_NavMeshAgent member after a pool relocation.
DP_BT_Nodes.h                     # Custom BT nodes (FindPosInSuspicionSphere /
                                  #   HasInvestigatePos / ClearInvestigatePos / Apprehend...).
                                  #   NOT components -- plain BT leaf classes.
```

### Visual / system components

```
DPFogPass_Component.h             # Per-frame fog-hole rebuild. ClearAllFogHoles at start of
                                  #   frame; RegisterFogHole per villager + per scene light.
                                  #   Owns the fog-hole + memory-fog tables behind DP_Fog.
DPProcLevelBootstrap_Component.h  # ProcLevel scene's bootstrap (impl in matching .cpp).
                                  #   OnAwake calls DPProcLevel::Generate(seed, cfg) + spawns
                                  #   one entity per layout element (room walls, doors,
                                  #   chests, forge, pentagram, item spawners, villagers,
                                  #   priest, patrol nodes). Seed from DP_PROCGEN_SEED env
                                  #   var if set.
```

## Behaviour Graphs & shims

The interactable/menu LOGIC lives in six boot-authored graphs (regenerated
every tools boot by `AuthorBehaviourGraphs()` in DevilsPlayground.cpp; see the
game-root CLAUDE.md for the graph table and `Zenith/Scripting/CLAUDE.md` for
the runtime + conversion playbook). The split:

- **Shims (C++, this directory):** input plumbing + systems execution.
  `DPGraphInteractable_Component` and `DPMenuRelay_Component` only fire custom
  events; `DPDoor_Component` additionally owns the door's navmesh/collider/
  tint/rotation systems, called back synchronously by the door nodes.
- **Nodes (`DP_GraphNodes.h`):** each `Execute` body is the retired C++
  handler VERBATIM (same DP_* calls, same analytics events, same guards;
  tuning read LIVE via key-string node params). Observable state (isOpen,
  openT, door anim) lives on graph blackboards; tests read it via
  `Tests/DP_TestGraphHelpers.h`.

| Node | Replaced C++ |
|---|---|
| `DPResetWinState` | DPPentagram OnAwake win-state reset |
| `DPDepositHeldObjective` | DPPentagram HandleInteract |
| `DPEmitNoise` | DummyNoiseMachine HandleInteract |
| `DPTryOpenDoor` / `DPAnimateDoorLeaves` | DPDoubleDoor interact / OnUpdate |
| `DPOpenChest` / `DPAdvanceChestLid` | DPChest interact / OnUpdate |
| `DPRequestQuit` | DPMainMenuController OnQuitClicked |
| `DPDoorHandleInteract` / `DPDoorAdvanceAnim` | DPDoor HandleInteract / OnUpdate animation |

Registered via `DP_RegisterGraphNodes()` from `Project_RegisterGameComponents`.
The deleted components (DPPentagram / DPChest / DPDoubleDoor /
DummyNoiseMachine / DPMainMenuController) have NO C++ remnants — equivalence
was proven by characterization tests that pass identically pre/post conversion
(DP suite 138/138).

## The component pattern

Every game component follows the same skeleton:

```cpp
#pragma once

class DPFoo_Component ZENITH_FINAL
{
public:
    DPFoo_Component() = delete;
    DPFoo_Component(Zenith_Entity& xParentEntity) : m_xParentEntity(xParentEntity) {}

    void OnAwake()            { /* register side-tables, cache cross-refs */ }
    void OnStart()            { /* subscribe to events, kick state machines */ }
    void OnUpdate(float fDt)  { /* per-frame logic */ }
    void OnDisable()          { /* unsubscribe -- CRITICAL for subscribers */ }
    void OnDestroy()          { /* deregister side-tables */ }

    // REQUIRED by the component contract (even if trivial):
    void WriteToDataStream(Zenith_DataStream& xStream) const { const u_int uVersion = 1; xStream << uVersion; }
    void ReadFromDataStream(Zenith_DataStream& xStream) { u_int uVersion = 0; xStream >> uVersion; }
#ifdef ZENITH_TOOLS
    void RenderPropertiesPanel() {}
#endif

private:
    Zenith_Entity m_xParentEntity;  // explicit member (the old script base provided it)
};
```

Registration is two-part, both in `DevilsPlayground.cpp`:

1. File-scope `ZENITH_REGISTER_COMPONENT(DPFoo_Component, "DPFoo", <order>)`
   macro (orders 100-119, with gaps at 105, 113, 114, 116, 117; currently 15
   components registered). The macro enqueues a thunk drained by the
   meta-registry seal; post-seal registration re-finalizes, so either order
   is safe.
2. Editor mirror in `Project_RegisterGameComponents()`:
   `Zenith_ComponentEditorRegistry::Get().RegisterComponent<DPFoo_Component>("DPFoo")`
   under `ZENITH_TOOLS` — this display name is what
   `AddStep_AddComponent("DPFoo")` and the editor menu resolve.

### Heap-stability (pool relocation) rules

Component pools RELOCATE instances (resize, swap-and-pop removal,
cross-scene transfer all move-construct + destruct the source). Every DP
component is therefore move-constructible, and any component with state
that points at `this` hand-writes its moves:

- **Singletons** (`s_pxInstance`: DPPlayerController, DPItemManager,
  DPFogPass, DPProcLevelBootstrap; `s_pxPersistentInstance`:
  DPPauseMenuController) — the move repoints the static at the new
  address; the destructor only clears it when it still points at `this`.
- **this-capturing event subscriptions** (DPInteractable_Base's
  DP_OnInteract handler; DPHUDController's five handlers;
  DPPauseMenuController's two) — the move unsubscribes the source's
  handles and re-subscribes fresh lambdas capturing the new `this`
  (the subscription body is factored into a single method per class).
- **Pointed-at by-value members** (Priest_Component's
  `Zenith_NavMeshAgent m_xNavAgent`, wired into the AIAgentComponent via
  `SetNavMeshAgent(&m_xNavAgent)`) — the move re-wires the pointer.

Components with only trivially-movable members (DPVillager, DPOrbitCamera,
DPItemBase, DPItemSpawn, DPMenuRelay, DPGraphInteractable)
rely on their implicit moves — do NOT add a user-declared destructor to
them without re-checking movability.

### Runtime attach + the OnAwake gap

`Prefab::Instantiate` dispatches the entity's OnAwake wave (and marks the
entity awoken) before returning, so a game component added AFTER an
Instantiate never receives OnAwake from the lifecycle scheduler. Every
post-Instantiate attach site (`DPProcLevelBootstrap_Component.cpp`,
`DPItemManager_Component::SpawnItemEntity`,
`DPForge_Component::SpawnOutputItem`) therefore calls the component's
`OnAwake()` explicitly right after `AddComponent<T>()` — preserving the
old script system's immediate-OnAwake-on-attach semantics. OnStart still
arrives via the normal pending-start dispatch (which iterates the
entity's components at dispatch time, so late-added components are
included).

### Cross-component communication

A component can read another component's outputs via:

- **The DP_* namespaces** (`DP_Player`, `DP_Items`, `DP_Win`, etc) —
  preferred.
- **Events via Zenith_EventDispatcher** — `DP_OnInteract`,
  `DP_OnVictory`, `DP_OnRunLost`, `DP_OnVillagerDied`, `DP_OnBellRing`,
  `DP_OnInteractionEnd`, etc.
- **`DP_Query::ForEachComponentInActiveScene<T>(lambda)`** — find every
  component of type T in the active scene (a thin wrapper over the
  scene's `Query<T>()`; `ForEachComponentInLoadedScenes<T>` is the
  all-loaded-scenes variant).

Components do **NOT** include each other's headers. That's the
contract.

**Documented exceptions (creation/lifecycle, not cross-state reads).** A few
components legitimately include sibling component headers because they
*create* / attach them — something a flat `DP_*` forwarder can't express
cleanly: `DPProcLevelBootstrap_Component` (the scene's master spawn-factory —
instantiates one entity per procgen layout element), `DPItemManager_Component`
(iterates its own `DPItemSpawn` anchors and instantiates `DPItemBase` items),
and `DPForge_Component` (spawns an item entity and attaches `DPItemBase`). Each
such include is marked `// Contract exception` in code (grep for it). Every
*read* cross-query goes through a `DP_*` forwarder instead (e.g.
`DP_Win::IsPentagramInRange`, `DP_Player::IsBeggarVillager`,
`DP_AI::GetNearestPriestDistanceFrom`,
`DP_Interactables::FindNearestInteractableType`).

### Subscription cleanup discipline

If a component subscribes to an event via a captured lambda
(`Subscribe<DP_OnFoo>([this](const DP_OnFoo&){ ... })`), it MUST
unsubscribe in OnDisable + OnDestroy, AND transfer the subscription in
its hand-written moves. The lambda captures `this`; if the entity is
destroyed mid-range (or the pool relocates the component), the captured
pointer dangles and the dispatcher crashes on next dispatch.
`DPInteractable_Base` is the canonical example — it stores the exact
subscription handle on rising-edge and unsubscribes that exact handle.

## Adding a new component

1. Drop a header under `Components/` matching the pattern above (or
   `: public DPInteractable_Base` for a proximity interactable).
2. `#include` it from `DevilsPlayground.cpp` and add a file-scope
   `ZENITH_REGISTER_COMPONENT(DPFoo_Component, "DPFoo", <next free order>)`
   plus the TOOLS editor-registry mirror in
   `Project_RegisterGameComponents()`.
3. Run Sharpmake (`cmd /c '.\Sharpmake_Build.bat < nul'` from `Build/`)
   to regenerate the vcxproj with the new file.
4. Reference from authoring:
   - Hand-authored scenes: `AddStep_AddComponent("DPFoo")`
     inside `Project_RegisterEditorAutomationSteps`.
   - Procgen scenes: have `DPProcLevelBootstrap` spawn it at runtime
     from a new procgen layout element type (remember the explicit
     `OnAwake()` call after a post-Instantiate `AddComponent`).

## Known gotchas

- **Interactable subscribers must clean up.** The OnExitRange
  unsubscribe path is the most common source of dangling-lambda
  crashes if you forget.
- **Procgen geometry is not tuned for hand-tuned priest tests.** Tests
  that need a specific priest/villager spatial arrangement may fail
  on procgen because the navmesh agent steers the priest back to its
  nearest patrol-node polygon. Stage such tests by spawning fresh
  entities at known clear positions rather than teleporting
  procgen-spawned entities. (See Tests/CLAUDE.md.)
- **`Project_RegisterEditorAutomationSteps` only runs in tools
  builds.** Wrapped in `#ifdef ZENITH_TOOLS`. Non-tools builds load
  pre-saved `.zscen` files; re-run the tools build after changing
  scene authoring or components.
