# Combat Arena Game

An arena-based combat game demonstrating Animation State Machines, Inverse Kinematics, and Event-driven damage systems.

## Engine Features Demonstrated

| Feature | Engine Class | Usage |
|---------|--------------|-------|
| **Animation State Machine** | `Flux_AnimationStateMachine` | Complex combat animation states with transitions |
| **Animation Parameters** | `Flux_AnimationParameters` | Float/Bool/Trigger parameters for state control |
| **Inverse Kinematics** | `Flux_IKSolver`, `SolveLookAtIK` | Foot placement IK and head look-at |
| **Event System** | `Zenith_EventDispatcher` | Custom damage/death events with deferred dispatch |
| **Entity Queries** | `Zenith_Query` | Finding enemies within attack radius |
| **Physics Hit Detection** | `Zenith_ColliderComponent` | Capsule colliders for characters |
| **Script Behaviours** | `Zenith_ScriptBehaviour` | OnCollisionEnter for hit registration |
| **DataAsset Configuration** | `Zenith_DataAsset` | Combat tuning parameters |
| **Multi-Scene Management** | `Zenith_SceneManager` | `DontDestroyOnLoad()`, `CreateEmptyScene()`, `UnloadScene()`, `SetScenePaused()` |
| **UI Buttons** | `Zenith_UIButton` | Clickable/tappable menu buttons with `SetOnClick()` callback |
| **Timed Destruction** | `Zenith_SceneManager::Destroy` | Corpse auto-cleanup with delayed entity destruction |

## File Structure

```
Games/Combat/
  CLAUDE.md                      # This documentation
  Combat.cpp                     # Project entry points, resource initialization
  Components/
    Combat_Config.h              # DataAsset for combat tuning
    Combat_Behaviour.h           # Main game coordinator
    Combat_PlayerController.h    # Movement + combat input handling
    Combat_AnimationController.h # Animation state machine wrapper
    Combat_IKController.h        # Foot placement + look-at IK
    Combat_HitDetection.h        # Physics-based hit detection
    Combat_DamageSystem.h        # Event-based damage and death
    Combat_EnemyAI.h             # Simple enemy behavior (chase + attack)
    Combat_QueryHelper.h         # Entity query utilities
    Combat_UIManager.h           # Health bars + combo display
  Assets/
    Scenes/Arena.zscen           # Serialized scene
```

## Module Breakdown

### Combat.cpp - Entry Points
**Engine APIs:** `Project_GetName`, `Project_RegisterScriptBehaviours`, `Project_CreateScenes`, `Project_LoadInitialScene`

Demonstrates:
- Procedural capsule geometry generation for characters
- Cube geometry for arena floor/walls
- Material creation for player/enemy/arena
- Prefab creation for runtime instantiation
- Event subscription for damage system

### Combat_Behaviour.h - Main Coordinator
**Engine APIs:** `Zenith_ScriptBehaviour`, lifecycle hooks

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
**Engine APIs:** `Flux_AnimationStateMachine`, `Flux_AnimationParameters`, `Flux_AnimationState`

Demonstrates:
- Creating combat animation states (Idle, Walk, LightAttack1-3, HeavyAttack, Dodge, Hit, Death)
- Parameter-driven transitions (Speed float, IsAttacking bool, AttackTrigger trigger)
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

## Multi-Scene Architecture

### Entity Layout
Persistent scene holds GameManager entity (Camera + UI + Combat_Behaviour) with DontDestroyOnLoad. Arena scene holds level entities (arena floor/walls, player, enemies), created/destroyed on transitions.

### Game State Machine
```
MAIN_MENU → PLAYING → PAUSED → GAME_OVER → MAIN_MENU
```

### Scene Transition Pattern
Uses `CreateEmptyScene("Arena")` + `SetActiveScene()` to start, `UnloadScene()` to return to menu, and `SetScenePaused()` for pausing.

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
    +---------+   +-----------+  +---------+      |
    |LightAtk1|-->|LightAtk2  |->|LightAtk3|------+
    +---------+   +-----------+  +---------+      |
                        |                         |
                        v                         |
                  +-----------+                   |
                  | HeavyAtk  |-------------------+
                  +-----------+
                        |
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
Create `Flux_AnimationStateMachine`, add parameters (Float, Bool, Trigger types), add states, and configure transitions with conditions.

### IK Application
Create IK chains via `Flux_IKSolver::CreateLegChain()`, set targets from raycasts each frame, and call `Solve()` after animation evaluation.

### Event-Based Damage
Define custom event structs (e.g., `Combat_DamageEvent`), subscribe via `Zenith_EventDispatcher::Get().SubscribeLambda<>()`, and dispatch events on hit.

### Entity Queries
Query entities via `xScene.Query<ComponentA, ComponentB>().ForEach()` with lambda, filtering by distance or tag.

## Editor View (What You See on Boot)

When launching in a tools build (`vs2022_Debug_Win64_True`):

### Scene Hierarchy
- **GameManager** - Persistent entity (Camera + UI + Script) - DontDestroyOnLoad
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
```
[Combat] Initializing combat arena
[Combat] Registering damage event handlers
[Combat] Player spawned at center
[Combat] Spawned 3 enemies
```

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
