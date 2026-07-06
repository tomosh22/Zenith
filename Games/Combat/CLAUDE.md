# Combat Arena Game

An arena-based combat game demonstrating Animation State Machines, Inverse Kinematics, and Event-driven damage systems.

## Engine Features Demonstrated

| Feature | Engine Class | Usage |
|---------|--------------|-------|
| **Animator Component** | `Zenith_AnimatorComponent` | Separate animation component (auto-discovers skeleton from ModelComponent) |
| **Animation State Machine** | `Flux_AnimationStateMachine` | Complex combat animation states with transitions, any-state transitions |
| **Animation Parameters** | `Flux_AnimationParameters` | Float/Bool/Trigger parameters for state control |
| **AnimatorStateInfo** | `Flux_AnimatorStateInfo` | Runtime state introspection (normalized time, state name) |
| **Inverse Kinematics** | `Flux_IKSolver`, `SolveLookAtIK` | Foot placement IK and head look-at |
| **Event System** | `Zenith_EventDispatcher` | Custom damage/death events with deferred dispatch |
| **Entity Queries** | `Zenith_Query` | Finding enemies within attack radius |
| **Physics Hit Detection** | `Zenith_ColliderComponent` | Capsule colliders for characters |
| **Game Components** | `Zenith_ComponentMetaRegistry` | Game logic via component lifecycle hooks (OnAwake, OnStart, OnUpdate) |
| **DataAsset Configuration** | `Zenith_DataAsset` | Combat tuning parameters |
| **Multi-Scene Management** | `Zenith_SceneSystem` (`g_xEngine.Scenes()`) | `DontDestroyOnLoad()`, `LoadScene(..., SCENE_LOAD_ADDITIVE_WITHOUT_LOADING)`, `UnloadScene()`, `SetScenePaused()` |
| **UI Buttons** | `Zenith_UIButton` | Clickable/tappable menu buttons with `SetOnClick()` callback |
| **Timed Destruction** | `Zenith_Entity::Destroy(delay)` | Corpse auto-cleanup with delayed entity destruction |

## File Structure

```
Games/Combat/
  CLAUDE.md                      # This documentation
  Combat.cpp                     # Project entry points, resource initialization
  Components/
    Combat_Config.h              # DataAsset for combat tuning
    Combat_GameComponent.h       # Main game coordinator component
    Combat_PlayerComponent.h/.cpp # Per-entity player component (controller/anim/IK/hit detection)
    Combat_EnemyComponent.h/.cpp # Per-entity enemy component (wraps Combat_EnemyAI)
    Combat_PlayerController.h    # Movement + combat input handling
    Combat_AnimationController.h # Animation state machine wrapper
    Combat_IKController.h        # Foot placement + look-at IK
    Combat_HitDetection.h        # Physics-based hit detection
    Combat_DamageSystem.h        # Event-based damage and death
    Combat_EnemyAI.h             # Simple enemy behavior (chase + attack)
    Combat_QueryHelper.h         # Entity query utilities
    Combat_UIManager.h           # Health bars + combo display
    Combat_GraphNodes.h          # Behaviour Graph node library (all 5 graphs)
  Tests/
    Test_CombatCharacterization.cpp  # 14 automated tests (attack/heavy, player combo/dodge/hit-stun, enemy engage/hit-stun, victory/game-over/combo-timer, pause/restart/menu/play)
  Assets/
    Scenes/MainMenu.zscen, Arena.zscen  # Boot-authored scenes
    Graphs/                      # Boot-authored Behaviour Graphs:
                                 #   Combat_PlayerAttack, Combat_PlayerState, Combat_EnemyBrain,
                                 #   Combat_RoundFlow, Combat_GameFlow (.bgraph)
```

## Module Breakdown

### Combat.cpp - Entry Points
**Engine APIs:** `Project_GetName`, `Project_RegisterGameComponents`, `Project_RegisterEditorAutomationSteps`, `Project_LoadInitialScene`

Demonstrates:
- Procedural capsule geometry generation for characters
- Cube geometry for arena floor/walls
- Material creation for player/enemy/arena
- Prefab creation for runtime instantiation
- Event subscription for damage system

### Combat_GameComponent.h - Main Coordinator
**Engine APIs:** `Zenith_ComponentMetaRegistry` (registered as "CombatGame", order 100; player/enemy components at 101/102), lifecycle hooks

Demonstrates:
- Coordinator pattern delegating to specialized modules
- Game state machine (Playing/Paused/GameOver)
- Round-based combat management
- Integration of animation, IK, and damage systems

### Combat_PlayerController.h - Player Input
**Engine APIs:** `Zenith_Input`, `Zenith_Physics`

Demonstrates:
- WASD movement with physics-based character controller
- Attack input (light attack on left click, heavy attack on right click)
- Dodge/roll on space bar
- State-based input blocking during attacks/dodge

### Combat_AnimationController.h - Animation State Machine
**Engine APIs:** `Zenith_AnimatorComponent`, `Flux_AnimationStateMachine`, `Flux_AnimationParameters`, `Flux_AnimationState`

Demonstrates:
- Using `Zenith_AnimatorComponent` (separate from `Zenith_ModelComponent`) for animation
- Creating combat animation states (Idle, Walk, Attack1-3, Dodge, Hit, Death)
- Parameter-driven transitions (Speed float, AttackTrigger trigger, DodgeTrigger trigger, HitTrigger trigger, DeathTrigger trigger)
- Any-State transitions for Hit and Death (fire from any current state)
- `AnimatorStateInfo` queries for hit-frame detection (`IsAttackHitFrame()`)
- Combo system with timed transition windows
- Exit time conditions for attack recovery

### Combat_IKController.h - Inverse Kinematics
**Engine APIs:** `Flux_IKSolver`, `SolveLookAtIK`, `Flux_IKChain`

Demonstrates:
- Foot placement IK using raycasts to ground
- Look-at IK for head tracking nearest enemy
- Blending IK with animation poses
- Disabling IK during certain states (dodge, death)

### Combat_HitDetection.h - Hit Detection
**Engine APIs:** `Zenith_ColliderComponent`, `Zenith_Query`

Demonstrates:
- Physics-based hit detection via collision callbacks
- Attack hitbox activation during attack frames
- Hit registration with cooldown to prevent multi-hits
- Distance-based hit validation

### Combat_DamageSystem.h - Event-Based Damage
**Engine APIs:** `Zenith_EventDispatcher`, custom events

Demonstrates:
- Custom event structs (Combat_DamageEvent, Combat_DeathEvent)
- Event subscription with lambda callbacks
- Immediate dispatch for damage application
- Health tracking and death state

### Combat_EnemyAI.h - Simple AI
**Engine APIs:** `Zenith_Query`, `Zenith_Physics`

Demonstrates:
- Finding player entity via query
- Simple chase behavior with arrival distance
- Attack decision based on range and cooldown
- Knockback response to player attacks

### Combat_QueryHelper.h - Query Utilities
**Engine APIs:** `Zenith_Query`, `Zenith_Scene`

Demonstrates:
- Finding entities within radius
- Filtering by component type
- Sorting by distance
- Tag-based entity identification

### Combat_UIManager.h - HUD
**Engine APIs:** `Zenith_UIComponent`, `Zenith_UIText`

Demonstrates:
- Health bar display for player
- Combo counter with timeout
- Enemy health indicators
- Game over / victory screens

## Behaviour Graphs (all game DECISION logic — 5 graphs)

Every piece of combat DECISION logic lives in behaviour graphs; the C++
components keep only SYSTEMS (physics, IK, damage math, HUD render, scene
mgmt, hit overlap) behind small graph-facing shims. Graphs are boot-authored
via a fluent `Zenith_GraphBuilder` in `BuildGraph_Combat*` functions
(`AddStep_GraphBuild` in `Project_RegisterEditorAutomationSteps`; runtime docs
in `Zenith/Scripting/CLAUDE.md`). The components fire the driving custom events
at EXACTLY the points the old bodies ran, and each event runs its graph
SYNCHRONOUSLY, so per-frame ordering is byte-identical. dt rides the event
PAYLOAD (custom-event context dt is 0).

| Graph | Attached | Driven by | Decomposition |
|---|---|---|---|
| `Combat_PlayerAttack.bgraph` | runtime, `CreateArena` (`AddGraphByAssetPath`, player) | `"AttackTick"` (end of `Combat_PlayerComponent::OnUpdate`, no payload) | Old `CombatAttackFlow` mega-node DECOMPOSED into 3 `"AttackTick"` chains (node-order = old guard order): `CombatQueryAttackState`→`Branch`→`CombatActivateHitbox` (10/1.5 light, 25/2.0 heavy props) / `Branch`→`CombatRegisterHits`→`CompareBlackboardInt`→`CombatNotifyComboHit` / `Branch`→`CombatDeactivateHitbox` |
| `Combat_PlayerState.bgraph` | runtime, `CreateArena` (2nd graph on the player) | `"PlayerTick"` (where `m_xController.Update` ran, dt payload) | Old `Combat_PlayerController::Update` switch DELETED → `CombatPlayerPreTick`→`StateMachine(playerState, 9 states)`→per-state handler (`Movement`/`Attack`/`Dodge`/`HitStun`; DEAD unwired)→`CombatPlayerPostTick` |
| `Combat_EnemyBrain.bgraph` | runtime, `SpawnEnemies` (`AddGraphByAssetPath`, per enemy) | `"EnemyBrainTick"` (`Combat_EnemyComponent::OnUpdate`, dt payload) | Old `Combat_EnemyAI::Update` switch DELETED → `CombatEnemyPreTick`→`StateMachine(enemyState, 5 states)`→per-state handler (`Idle`/`Chase`/`Attack`/`HitStun`; DEAD unwired)→`CombatEnemyPostTick` |
| `Combat_RoundFlow.bgraph` | authored, Arena GameManager | `"RoundTick"` (PLAYING branch of `Combat_GameComponent::OnUpdate`, dt payload) | `CombatTickComboTimer` (kept — combo state is a cross-entity static) then old `CombatCheckRoundState` mega-node DECOMPOSED: `CombatCountAliveEnemies`→`CompareBlackboardInt`→`CombatSetGameState(VICTORY)`; `CombatCheckPlayerDead`→`CombatSetGameState(GAME_OVER)` (VICTORY-before-GAME_OVER, independent) |
| `Combat_GameFlow.bgraph` | authored, BOTH GameManagers | order-60 input sources (`OnKeyPressed`/`OnUIButtonClicked`/`OnUpdate`) | Old `OnUpdate` menu/pause/reset switch made systems-only; the input DECISIONS moved here: P/R/Escape → `CombatGetGameState`→`SwitchOnInt(gameState)`→ pause/`CombatResetGame`/`CombatReturnToMenu`; menu Play → `OnUIButtonClicked`→`LoadSceneByIndex(1)`; focus → `OnUpdate`→`CombatFocusPlayButton` |

The old C++ decision bodies are all DELETED: `UpdateAttack`, `CheckGameState`,
`UpdateComboTimer`, `Combat_EnemyAI::Update` (+ the dead `Combat_EnemyManager`),
`Combat_PlayerController::Update`, and the `Combat_GameComponent::OnUpdate`
P/R/Escape switch. The C++ member state stays the source of truth for the two
state machines (`GetState()`/`GetComboCount()` unchanged); a `PreTick` node
mirrors it to the blackboard only to drive the `StateMachine` dispatch (no
Enter/Exit events, no RUNNING leaves). Shims: `Combat_PlayerComponent`/
`Combat_EnemyComponent` `Graph_*` forwarders, `Combat_GameComponent::SetGameState`/
`Graph_*` (public static / public). Nodes registered via
`Combat_RegisterGraphNodes()` from `Project_RegisterGameComponents`.

**Accepted divergences** (unobservable / precedented): `Combat_GameFlow`'s
`@60`-graph / `@100`-component split shifts the systems block by one frame on a
reset / resume frame (runs on fresh/resumed state); and P/R/Escape are
independent `OnKeyPressed` sources, so two distinct keys on the exact same frame
both fire (the old `if/else-if/return` switch was mutually exclusive). Combo
count/timer stay `Combat_GameComponent` statics (cross-entity shared: written by
the attack graph, ticked by the round graph, read by the HUD).

**Equivalence proof:** `Tests/Test_CombatCharacterization.cpp` — 14 tests
written against the C++ versions FIRST, all identical pre/post conversion. Run
WINDOWED (headless asserts on the skeletal model's GPU buffers):

```
combat.exe --all-automated-tests --exit-after-frames 30000 --fixed-dt 0.01666 --skip-unit-tests
```

Coverage: attack (`AttackFlow`=10 dmg/combo1/no-double-dip, `HeavyAttack`=25/
combo0), player state machine (`PlayerCombo`=LA1→LA2 chain, `PlayerDodge`,
`PlayerHitStun`), enemy brain (`EnemyEngages`=chase+attack+player-damage,
`EnemyHitStun`), round (`RoundVictory`/`RoundGameOver`/`ComboTimer`), game flow
(`PauseResume`/`Restart`/`ReturnToMenu`/`MenuPlay`).

> ★ `AttackFlow`/`HeavyAttack` steer to a RANDOM-spawned enemy (`SpawnEnemies`
> seeds `m_xRng` with `std::random_device`) amid combat, so they occasionally
> flake (the player gets ganged-up and killed → GAME_OVER freezes the steer →
> timeout). On a steer-timeout fail, RE-RUN; the state/round/gameflow tests are
> deterministic and never flake.

> Note: the menu→Play path (`LoadSceneByIndex(1)` → Arena GameComponent
> OnAwake → `StartGame` → additive `LoadScene`) depends on the engine's
> `SCENE_LOAD_ADDITIVE_WITHOUT_LOADING` fast path being exempt from the
> mid-dispatch deferral — see `Zenith_SceneSystem_Operations.cpp`.

## Multi-Scene Architecture

### Entity Layout
Persistent scene holds GameManager entity (Camera + UI + Combat_GameComponent), kept alive across transitions via the `SCENE_LOAD_ADDITIVE_WITHOUT_LOADING` flag on the Arena load. Arena scene holds level entities (arena floor/walls, player, enemies), created/destroyed on transitions.

### Game State Machine
```
MAIN_MENU → PLAYING → PAUSED → GAME_OVER → MAIN_MENU
```

### Scene Transition Pattern
Uses `LoadScene("Arena", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING)` + `SetActiveScene()` to start, `UnloadScene()` to return to menu, and `SetScenePaused()` for pausing.

## Learning Path

1. **Start here:** `Combat.cpp` - See resource initialization and event setup
2. **Understand animation:** `Combat_AnimationController.h` - State machine setup
3. **See IK:** `Combat_IKController.h` - Foot placement and look-at
4. **Learn events:** `Combat_DamageSystem.h` - Custom event dispatch
5. **Entity queries:** `Combat_QueryHelper.h` - Finding entities by criteria

## Controls

| Key | Action |
|-----|--------|
| W/A/S/D | Move player |
| Left Mouse | Light attack (combo chain) |
| Right Mouse | Heavy attack |
| Space | Dodge roll |
| Click / Touch | Select menu button |
| W/S or Up/Down | Navigate menu |
| Enter | Activate focused button |
| Escape | Return to menu / Pause |
| P | Pause/Resume |
| R | Restart round |

## Animation State Machine

```
                    +-------+
                    | Idle  |<--------------------+
                    +-------+                     |
                        |                         |
              Speed > 0 v                         |
                    +-------+                     |
                    | Walk  |                     |
                    +-------+                     |
                        |                         |
           AttackTrigger v                        |
          +-------------+-------------+           |
          |             |             |           |
          v             v             v           |
    +---------+   +---------+   +---------+        |
    | Attack1 |-->| Attack2 |-->| Attack3 |-------+
    +---------+   +---------+   +---------+        |
                        |                         |
              HitTrigger v
                    +-------+
                    |  Hit  |---------------------+
                    +-------+
                        |
              DeathTrigger v
                    +-------+
                    | Death |
                    +-------+
```

## Key Patterns

### Animation State Machine Setup
Add `Zenith_AnimatorComponent` to the entity (separate from `Zenith_ModelComponent`). The AnimatorComponent auto-discovers the skeleton from ModelComponent on the same entity during `OnStart()`. Create states and transitions via `GetStateMachine()`, use Any-State transitions for global reactions (Hit, Death), and query state with `GetCurrentAnimatorStateInfo()`.

### IK Application
Create IK chains via `Flux_IKSolver::CreateLegChain()` and add them to `xAnimator.GetController().GetIKSolver()`. Set targets each frame from raycasts via `Zenith_AnimatorComponent::SetIKTarget()` (or `SetIKTargetModelSpace()` for one-frame-latency-immune targets). The controller invokes `Flux_IKSolver::Solve()` automatically inside `ApplyOutputPoseToSkeleton` — do not call it directly when using the animator pipeline, or IK will run twice per frame.

### Event-Based Damage
Define custom event structs (e.g., `Combat_DamageEvent`), subscribe via `Zenith_EventDispatcher::Get().Subscribe<>()` (or `SubscribeScoped<>()` for RAII auto-unsubscribe), and dispatch events on hit.

### Entity Queries
Query entities via `xScene.Query<ComponentA, ComponentB>().ForEach()` with lambda, filtering by distance or tag.

## Editor View (What You See on Boot)

When launching in a tools build (`vs2022_Debug_Win64_True`):

### Scene Hierarchy
- **GameManager** - Persistent entity (Camera + UI + CombatGame component) - DontDestroyOnLoad
- **Player** - Player character entity with model, collider, and animation components
- **ArenaFloor** - Ground plane for the combat arena
- **ArenaWall_X** - Wall entities forming the arena boundary
- **Enemy_X** - Enemy character entities (spawned during gameplay)

### Viewport
- **Third-person perspective** view behind and above the player
- **Player character** (capsule/humanoid) at center of arena
- **Gray floor** representing the arena ground
- **Arena walls** forming a contained combat area
- **Enemy characters** (red-tinted capsules) at spawn positions

### Properties Panel (when CombatGame selected)
- **Combat section** - Attack damage, combo window, dodge cooldown
- **Animation section** - Transition durations, blend times
- **AI section** - Chase speed, attack range, reaction time
- **Debug section** - Show hitboxes, FPS counter

### Console Output on Boot
Resource setup logs the stick-figure mesh/model path (or the capsule fallback), e.g.:
```
[Combat] Loaded stick figure mesh from <path>
[Combat] Created model asset at <path>      (first boot)
[Combat] Using model asset at <path>         (subsequent boots)
[Combat] Stick figure assets not found (...), using capsule fallback
```
On shutdown: `[Combat] Resources cleaned up`.

## Gameplay View (What You See When Playing)

### Initial State
- Player character at arena center in idle pose
- 3 enemy characters positioned around the arena
- Camera behind player, looking toward center
- All health bars at full

### HUD Elements
- **Player Health Bar** (Top-Left) - Green bar showing player HP
- **Combo Counter** (Center) - Shows current combo count (fades after timeout)
- **Enemy Health** - Small bars above each enemy
- **Round Info** (Top-Right) - "Round 1" and enemy count remaining

### Gameplay Actions
1. **Movement**: WASD moves player relative to camera
2. **Light Attack**: Left click performs quick attack (combo chain)
3. **Heavy Attack**: Right click performs slow powerful attack
4. **Dodge**: Space bar performs evasive roll
5. **Combo System**: Chaining attacks within timing window increases combo

### Visual Feedback
- Attack animations play with physics-based hit reactions
- Damage numbers appear on hit
- Health bars decrease on damage
- Screen flash on player damage
- Slow-motion effect on final enemy kill
- "VICTORY!" or "DEFEAT" overlay on round end

## Test Plan

### T1: Boot and Initialization
| Step | Action | Expected Result |
|------|--------|-----------------|
| T1.1 | Launch combat.exe | Window opens with third-person view of arena |
| T1.2 | Check player spawn | Player at arena center in idle pose |
| T1.3 | Check enemy spawns | 3 enemies visible at starting positions |
| T1.4 | Verify HUD | Health bars and round info visible |

### T2: Player Movement
| Step | Action | Expected Result |
|------|--------|-----------------|
| T2.1 | Press W | Player moves forward |
| T2.2 | Press S | Player moves backward |
| T2.3 | Press A | Player strafes/turns left |
| T2.4 | Press D | Player strafes/turns right |
| T2.5 | Move to arena edge | Player collides with wall, cannot exit |
| T2.6 | Move toward enemy | Player approaches enemy |

### T3: Combat - Light Attacks
| Step | Action | Expected Result |
|------|--------|-----------------|
| T3.1 | Left click near enemy | Light attack animation plays |
| T3.2 | Attack connects | Enemy takes damage, health bar decreases |
| T3.3 | Rapid left clicks | Combo chain (LightAtk1 -> LightAtk2 -> LightAtk3) |
| T3.4 | Check combo counter | Combo number increases and displays |
| T3.5 | Wait after combo | Combo counter resets after timeout |

### T4: Combat - Heavy Attack
| Step | Action | Expected Result |
|------|--------|-----------------|
| T4.1 | Right click near enemy | Heavy attack animation plays |
| T4.2 | Attack connects | Enemy takes significant damage |
| T4.3 | Check knockback | Enemy pushed back on heavy hit |

### T5: Dodge System
| Step | Action | Expected Result |
|------|--------|-----------------|
| T5.1 | Press Space | Dodge roll animation plays |
| T5.2 | Dodge during enemy attack | Player avoids damage |
| T5.3 | Spam Space | Dodge has cooldown, can't spam |
| T5.4 | Dodge while attacking | Attack cancelled, dodge executes |

### T6: Enemy AI
| Step | Action | Expected Result |
|------|--------|-----------------|
| T6.1 | Stand still | Enemies approach player |
| T6.2 | Enemy reaches range | Enemy attacks player |
| T6.3 | Move away from enemy | Enemy chases player |
| T6.4 | Attack enemy | Enemy reacts to hit (stagger) |

### T7: Damage and Death
| Step | Action | Expected Result |
|------|--------|-----------------|
| T7.1 | Get hit by enemy | Player health decreases |
| T7.2 | Kill an enemy | Enemy plays death animation, disappears |
| T7.3 | Kill all enemies | "VICTORY!" message, round ends |
| T7.4 | Player health reaches 0 | "DEFEAT" message, game over |

### T8: Win/Lose Conditions
| Step | Action | Expected Result |
|------|--------|-----------------|
| T8.1 | Defeat all enemies | Round complete, victory screen |
| T8.2 | Press R after victory | New round starts |
| T8.3 | Let enemies kill player | Game over screen |
| T8.4 | Press R after defeat | Game restarts |

### T9: Animation System
| Step | Action | Expected Result |
|------|--------|-----------------|
| T9.1 | Move then stop | Walk -> Idle transition smooth |
| T9.2 | Attack during walk | Attack animation interrupts walk |
| T9.3 | Get hit during attack | Hit reaction plays |
| T9.4 | Observe IK | Feet planted on ground correctly |

### T10: Edge Cases
| Step | Action | Expected Result |
|------|--------|-----------------|
| T10.1 | Attack while dodging | Cannot attack during dodge |
| T10.2 | Move during attack | Movement blocked during attack |
| T10.3 | Multiple enemies attack | All hits register properly |
| T10.4 | Pause during combat | Game pauses, all animations stop |

### T11: Menu and Scene Transitions
| Step | Action | Expected Result |
|------|--------|-----------------|
| T11.1 | Click menu button | Button activates, game starts |
| T11.2 | Navigate menu with W/S keys | Focus moves between buttons |
| T11.3 | Press Enter on focused button | Button activates |
| T11.4 | Start game from menu | Arena scene created, gameplay begins |
| T11.5 | Press Escape during gameplay | Returns to main menu, arena scene unloaded |
| T11.6 | Press P during gameplay | Game pauses, arena scene paused |
| T11.7 | Press P while paused | Game resumes |
| T11.8 | Restart via menu after game over | New arena scene created, level reset |

## Building

```batch
cd Build
Sharpmake_Build.bat
msbuild zenith_win64.sln /p:Configuration=vs2022_Debug_Win64_True /p:Platform=x64
cd ..\Games\Combat\Build\output\win64\vs2022_debug_win64_true
combat.exe
```
