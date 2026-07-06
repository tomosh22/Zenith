# Marble Roll Example Game

A physics-based ball rolling game demonstrating Jolt Physics integration and dynamic gameplay.

## Engine Features Demonstrated

| Feature | Engine Class | Usage |
|---------|--------------|-------|
| **Physics Integration** | `Zenith_Physics`, `Zenith_ColliderComponent` | Jolt Physics with dynamic/static bodies |
| **Collision Shapes** | `COLLISION_VOLUME_TYPE_SPHERE/AABB` | Sphere for ball, AABB for platforms |
| **Impulse-Based Movement** | `Zenith_Physics::AddImpulse` | Physics-based input response |
| **Camera Following** | `Zenith_CameraComponent` | Smooth follow with look-at |
| **Procedural Geometry** | `GenerateUVSphere()` (file-static helper in `Marble.cpp`) | Runtime sphere mesh generation into a `Flux_MeshGeometry` |
| **Game State Machine** | Behaviour Graph `StateMachine` node (`Marble_LevelFlow.bgraph`) | Playing/Paused/Won/Lost as blackboard int; `MarbleGameState` enum names the values C++-side |
| **Distance-Based Pickups** | Manual distance check | Simple collectible system |
| **Entity Lifetime** | `Zenith_Scene::Destroy` | Dynamic entity creation/destruction |
| **Multi-Scene Management** | `Zenith_SceneSystem` | `LoadScene(..., SCENE_LOAD_ADDITIVE_WITHOUT_LOADING)`, `SetActiveScene()`, `UnloadScene()`, `LoadSceneByIndex(..., SCENE_LOAD_SINGLE)`, `SetScenePaused()` |
| **UI Buttons** | `Zenith_UIButton` | Clickable/tappable menu buttons with `SetOnClick()` callback |

## File Structure

```
Games/Marble/
  CLAUDE.md                      # This documentation
  Marble.cpp                     # Project entry points, resource initialization
  Components/
    Marble_GameComponent.h       # Main coordinator component (uses modules below)
    Marble_Config.h              # DataAsset for game configuration (defined + serializable, but not yet instantiated/referenced; included in Marble.cpp only)
    Marble_Input.h               # Camera-relative physics input
    Marble_PhysicsController.h   # Ball movement via Jolt Physics
    Marble_CameraFollow.h        # Smooth camera tracking
    Marble_LevelGenerator.h      # Platform and collectible placement
    Marble_CollectibleSystem.h   # Pickup detection and scoring
    Marble_UIManager.h           # HUD management
    Marble_GraphNodes.h          # Behaviour Graph node library (5 systems-seam nodes)
  Tests/
    Test_MarbleCharacterization.cpp  # 7 automated tests (timer, fall, pause, reset, escape, menu, collection-win)
  Assets/
    Scenes/MainMenu.zscen, Marble.zscen        # Boot-authored scenes
    Graphs/Marble_LevelFlow.bgraph             # Gameplay flow (owns the game state)
    Graphs/Marble_GameFlow.bgraph              # Menu flow (Play click + focus)
```

## Behaviour Graphs (W1 conversion — ALL decisions graph-side)

Both graphs are boot-authored through `Zenith_GraphBuilder`
(`AddStep_GraphBuild`, `BuildGraph_Marble*` in Marble.cpp) and attached with
`AddStep_AttachGraph` (runtime docs in `Zenith/Scripting/CLAUDE.md`). The
game state LIVES on the graph blackboard — **the "state moves to the graph
blackboard, shim accessor reads it" pilot**: `Marble_GameComponent`'s
`GetGameState`/`GetTimeRemaining`/`GetScore`/`GetCollectedCount` read the
attached graph's blackboard (`gameState`/`timeRemaining`/`score`/`collected`);
the HUD, camera gating, and characterization tests consume state only through
those accessors. The component keeps SYSTEMS only (physics input, camera,
level gen, collectible detection, fall query, HUD text writes).

**`Marble_LevelFlow.bgraph`** (gameplay GameManager; declares
`gameState`=PLAYING, `timeRemaining`=60, `score`/`collected`=0):
- `OnStart` → 6× `SetUIVisible(true)` (the old StartGame HUD show).
- Four `OnCustomEvent("LevelTick")` chains (component fires it once per
  PLAYING frame with dt as payload; custom-event sources run in node order,
  preserving the old same-frame decision order):
  1. `MarbleStageFrameResults` (publishes scoreGained/collectedDelta/
     allCollected/ballFell from the systems pass),
  2. timer: `MathBlackboardFloat`(sub payload) → `ClampBlackboardFloat`(0) →
     `CompareBlackboardFloat`(≤0) → `Branch` → LOST,
  3. collection: `AddBlackboardInt`(score+=scoreGained) →
     `AddBlackboardInt`(collected+=collectedDelta) → `Branch`(allCollected) → WON,
  4. fall: `Branch`(ballFell) → LOST.
- `OnKeyPressed(P)` → `SwitchOnInt(gameState)`: PLAYING→PAUSED, PAUSED→PLAYING.
- `OnKeyPressed(Esc)` → `SwitchOnInt(gameState)`: PLAYING→PAUSED (the
  `WasPausePressed` P-or-Esc quirk, preserved); PAUSED→resume →
  `MarbleUnloadLevel` → `LoadSceneByIndex(0)`; WON/LOST→unload→menu.
- `OnKeyPressed(R)` → `CompareBlackboardInt`(≠PAUSED) → `Branch` →
  `MarbleRegenerateLevel` → reset time/score/collected/state.
- `OnUpdate` → **`StateMachine`**(gameState, prefix "Marble", LAST in node
  order so key-driven transitions land the same dispatch); its
  `MarbleEnter_Paused`/`MarbleExit_Paused` transition events drive
  `MarbleSetLevelPaused(true/false)` — the scene-pause side effect is
  decoupled from the key chains.

**`Marble_GameFlow.bgraph`** (menu GameManager; declares `gameState`=MAIN_MENU,
`focusIndex`=0):
- `OnUIButtonClicked("MenuPlay")` → `LoadSceneByIndex(1)` (the engine UIButton
  trampoline source — no C++ click wiring; keyboard Enter activation reaches
  it too). **MenuQuit is deliberately unwired** (pre-conversion quirk).
- W/Up/S/Down `OnKeyPressed` chains: `CompareBlackboardInt`(focusIndex==0) →
  `Branch` → `SetBlackboardInt` toggle (2 buttons ⇒ every direction toggles).
- `OnUpdate` → `MarbleApplyMenuFocus` (SetFocused visuals from focusIndex).

### Game node library (`Components/Marble_GraphNodes.h`, ~5 systems seams)

| Node | What it does |
|---|---|
| `MarbleStageFrameResults` | Publishes the frame's systems results (CollectionResult + fall query) onto the blackboard |
| `MarbleRegenerateLevel` | Unload old level scene + create fresh + run generator (state resets are graph-side) |
| `MarbleUnloadLevel` | Clear entity refs + unload the level scene |
| `MarbleSetLevelPaused` | `SetScenePaused` on the level scene (from the StateMachine transition events) |
| `MarbleApplyMenuFocus` | Applies blackboard focusIndex to the two menu buttons (visuals only) |

Registered via `Marble_RegisterGraphNodes()`. The old wave-2 verbatim-wrapped
decision nodes (`MarbleTickTimer`/`MarbleApplyCollection`/`MarbleCheckFall`)
are DELETED — their decisions are engine nodes now.

**Known quirk (pinned by tests, preserved by the conversion):** collectible 0
spawns on the START platform inside the ball's pickup radius, so every level
start auto-collects it within a couple of frames (score opens at 100).

**Equivalence proof:** `Tests/Test_MarbleCharacterization.cpp` (headless OK; 7
tests, all written green against the C++ decisions FIRST and passing unchanged
against the graphs): `Marble_TimerFlow_Test` (countdown rate ±1 s/600 frames,
clamp, LOST at expiry), `Marble_FallLoss_Test` (held-W roll-off → LOST, real
input path), `Marble_PauseResume_Test` (P toggles PAUSED/PLAYING, timer
freezes), `Marble_ResetFlow_Test` (R resets timer/score/collected; score
pre-inflated via the `Test_InjectCollection` seam), `Marble_EscapeMenu_Test`
(Esc pauses, Esc again returns to menu), `Marble_MenuPlay_Test` (initial focus
Play, W/S toggle, Quit inert, Play activation → PLAYING),
`Marble_CollectionWin_Test` (injected results accumulate; allCollected → WON).

```
marble.exe --all-automated-tests --headless --exit-after-frames 30000 --fixed-dt 0.01666 --skip-unit-tests
```

## Module Breakdown

### Marble.cpp - Entry Points
**Engine APIs:** `Project_GetName`, `Project_RegisterGameComponents`, `Project_RegisterEditorAutomationSteps`, `Project_LoadInitialScene` (scenes are built via `Zenith_EditorAutomation` steps in `Project_RegisterEditorAutomationSteps`, e.g. `AddStep_CreateScene`/`AddStep_SaveScene`)

Demonstrates:
- Procedural UV sphere generation with tangent calculation
- Multiple material creation for game objects
- Prefab creation for ball, platforms, collectibles, goal

### Marble_GameComponent.h - Main Coordinator
**Engine APIs:** `Zenith_ComponentMetaRegistry` (registered as "MarbleGame", order 100), lifecycle hooks

Demonstrates:
- Game state machine (enum-based states)
- Coordinator pattern (delegates to modules)
- Time-based gameplay (countdown timer)

### Marble_Input.h - Camera-Relative Input
**Engine APIs:** `Zenith_Input::IsKeyDown`, `Zenith_CameraComponent`

Demonstrates:
- Continuous input with IsKeyDown (vs WasKeyPressedThisFrame)
- Camera-relative movement direction calculation
- Projecting camera forward onto XZ plane

### Marble_PhysicsController.h - Physics Movement
**Engine APIs:** `Zenith_Physics`, `Zenith_ColliderComponent`, `Zenith_PhysicsBodyID`

Demonstrates:
- Getting/setting physics body velocity
- Applying impulses for movement
- Jump mechanics with velocity check (prevents double-jump)
- Fall detection via position check

### Marble_CameraFollow.h - Camera System
**Engine APIs:** `Zenith_CameraComponent::SetPosition/SetPitch/SetYaw`

Demonstrates:
- Smooth follow with linear interpolation (glm::mix)
- Look-at calculation from pitch/yaw angles
- Fixed offset behind target

### Marble_LevelGenerator.h - Level Creation
**Engine APIs:** `Zenith_Scene::Instantiate`, `Zenith_TransformComponent`, `Zenith_ColliderComponent`

Demonstrates:
- Prefab-based entity creation
- Component order matters: Transform -> Model -> Collider
- Procedural platform placement in circular pattern
- Random distribution with std::uniform_real_distribution

### Marble_CollectibleSystem.h - Pickup System
**Engine APIs:** `Zenith_Scene::Destroy`

Demonstrates:
- Distance-based collision detection (no physics callbacks needed)
- Entity destruction on collection
- Score and win condition tracking

### Marble_UIManager.h - HUD Updates
**Engine APIs:** `Zenith_UIComponent`, `Zenith_UIText`

Demonstrates:
- Dynamic text with formatting
- Color changes based on game state
- Multiple UI elements (Score, Time, Collected, Status)

## Multi-Scene Architecture

### Entity Layout
Each authored scene (MainMenu, Marble) carries its own GameManager entity
(Camera + UI + MarbleGame component + Graph component); the SINGLE-mode scene
loads swap them wholesale, so per-scene graph blackboards never coexist. The
Level scene holds platforms, collectibles, goal, and player ball,
created/destroyed on transitions. (The persistent scene is used only during
resource init for the prefab templates.)

### Game State Machine
```
MAIN_MENU → PLAYING → {PAUSED / WON / LOST} → MAIN_MENU
```

### Scene Transition Pattern
Uses `LoadScene("Level", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING)` + `SetActiveScene()` to start the level, `UnloadScene()` then `LoadSceneByIndex(0, SCENE_LOAD_SINGLE)` to return to the menu, and `SetScenePaused()` for pausing. Scenes themselves are boot-authored via `Zenith_EditorAutomation` (`AddStep_CreateScene`/`AddStep_SaveScene`) and loaded by `Project_LoadInitialScene`.

## Learning Path

1. **Start here:** `Marble.cpp` - See UV sphere generation
2. **Understand physics:** `Marble_PhysicsController.h` - Jolt integration
3. **See camera:** `Marble_CameraFollow.h` - Smooth follow pattern
4. **Level creation:** `Marble_LevelGenerator.h` - Prefab instantiation + physics
5. **Gameplay:** `Marble_CollectibleSystem.h` - Simple distance-based pickups

## Controls

| Key | Action |
|-----|--------|
| W / Up Arrow | Roll forward (camera-relative) |
| S / Down Arrow | Roll backward |
| A / Left Arrow | Roll left |
| D / Right Arrow | Roll right |
| Space | Jump |
| Click / Touch | Select menu button |
| W/S or Up/Down | Navigate menu |
| Enter | Activate focused button |
| Escape | Return to menu / Pause |
| P | Pause/Resume |
| R | Reset level |

## Key Patterns

### Physics-Based Movement
Get `Zenith_PhysicsBodyID` from `Zenith_ColliderComponent`, apply forces via `Zenith_Physics::AddImpulse()`, and check velocity via `GetLinearVelocity()` for jump gating.

### Component Order for Physics
Entity creation order matters: 1) Instantiate from prefab, 2) Set transform position/scale, 3) Add ModelComponent, 4) Add ColliderComponent last (reads transform on creation).

### Camera-Relative Input Direction
Project camera-to-target vector onto XZ plane (zero Y, normalize) to get forward direction, then cross product with up vector for right direction.

## Editor View (What You See on Boot)

When launching in a tools build (`vs2022_Debug_Win64_True`):

### Scene Hierarchy
- **GameManager** - Persistent entity (Camera + UI + MarbleGame component) - lives in the engine's persistent scene
- **Ball** - The player-controlled marble with physics
- **Platform** - Initial platform where player spawns
- **Goal** - Final destination platform
- **Platform_X** - Multiple floating platform entities
- **Collectible_X** - Coin/gem pickup entities scattered on platforms

### Viewport
- **Third-person perspective** view behind and above the marble
- **Blue sphere** (player marble) on the starting platform
- **Gray rectangular platforms** floating in space
- **Yellow/gold spheres** as collectible items on platforms
- **Green platform** marking the goal destination
- **Dark background** representing the void below

### Properties Panel (when MarbleGame selected)
- **Player section** - Move speed, jump force, physics settings
- **Camera section** - Follow distance, smoothing speed
- **Timer section** - Countdown duration
- **Debug section** - FPS, physics body count

### Console Output on Boot
```
[Marble] Initializing physics world
[Marble] Created X platforms, Y collectibles
[Marble] Player ball spawned at start position
```

## Gameplay View (What You See When Playing)

### Initial State
- Player marble on the starting platform
- Camera positioned behind and above the ball
- Timer counting down from 60 seconds (or configured time)
- Collectibles visible on nearby platforms

### HUD Elements (Top-Left)
- **"Score: 0"** - Current score from collectibles
- **"Time: 60.0"** - Countdown timer in seconds
- **"Collected: 0 / 5"** - Collectibles gathered vs total

### Gameplay Actions
1. **Rolling**: Ball responds to physics impulses from input
2. **Jumping**: Ball launches upward when Space pressed (if grounded)
3. **Collecting**: Approaching a collectible within range destroys it and adds score
4. **Winning**: Reaching the goal platform shows "You Win!" message
5. **Losing**: Falling off platforms or timer reaching 0 shows "Game Over"

### Visual Feedback
- Ball rolls realistically with physics
- Camera smoothly follows ball movement
- Collectibles spin/rotate to attract attention
- Platforms have distinct visual style from void

## Test Plan

### T1: Boot and Initialization
| Step | Action | Expected Result |
|------|--------|-----------------|
| T1.1 | Launch marble.exe | Window opens with third-person view of ball on platform |
| T1.2 | Check physics initialization | Ball rests on platform without falling through |
| T1.3 | Verify camera position | Camera positioned behind and above the ball |
| T1.4 | Check HUD | Score, Time, and Collected counters visible |

### T2: Physics Movement
| Step | Action | Expected Result |
|------|--------|-----------------|
| T2.1 | Press W | Ball rolls forward (away from camera) |
| T2.2 | Press S | Ball rolls backward (toward camera) |
| T2.3 | Press A | Ball rolls left relative to camera |
| T2.4 | Press D | Ball rolls right relative to camera |
| T2.5 | Release all keys | Ball gradually slows due to friction |
| T2.6 | Roll to platform edge | Ball can fall off into void |

### T3: Jump Mechanics
| Step | Action | Expected Result |
|------|--------|-----------------|
| T3.1 | Press Space on platform | Ball jumps upward |
| T3.2 | Press Space while in air | No additional jump (prevents double-jump) |
| T3.3 | Land on another platform | Ball can jump again |
| T3.4 | Jump across gap | Ball can cross to adjacent platform |

### T4: Collectible System
| Step | Action | Expected Result |
|------|--------|-----------------|
| T4.1 | Roll near collectible | Collectible disappears when within range |
| T4.2 | Check score after collect | Score increments appropriately |
| T4.3 | Check collected counter | "Collected: X/Y" updates |
| T4.4 | Collect all items | Counter shows all collected |

### T5: Win/Lose Conditions
| Step | Action | Expected Result |
|------|--------|-----------------|
| T5.1 | Roll onto goal platform | "You Win!" message appears |
| T5.2 | Fall off platforms | "Game Over" after falling far enough |
| T5.3 | Let timer reach 0 | "Game Over - Time's Up!" message |
| T5.4 | Press R after game end | Level resets to initial state |

### T6: Camera System
| Step | Action | Expected Result |
|------|--------|-----------------|
| T6.1 | Move ball around | Camera follows smoothly behind |
| T6.2 | Jump high | Camera adjusts to keep ball in view |
| T6.3 | Move quickly | Camera catches up without jarring motion |

### T7: Pause System
| Step | Action | Expected Result |
|------|--------|-----------------|
| T7.1 | Press P or Escape | Game pauses, "PAUSED" displayed |
| T7.2 | Move while paused | Ball does not respond to input |
| T7.3 | Timer while paused | Timer does not decrement |
| T7.4 | Press P again | Game resumes from paused state |

### T8: Edge Cases
| Step | Action | Expected Result |
|------|--------|-----------------|
| T8.1 | Roll very fast | Physics remains stable |
| T8.2 | Collide with platform edge | Ball bounces or stops correctly |
| T8.3 | Jump at platform edge | Ball can land back on same platform |
| T8.4 | Minimize/restore window | Game resumes correctly |

### T9: Menu and Scene Transitions
| Step | Action | Expected Result |
|------|--------|-----------------|
| T9.1 | Click menu button | Button activates, game starts |
| T9.2 | Navigate menu with W/S keys | Focus moves between buttons |
| T9.3 | Press Enter on focused button | Button activates |
| T9.4 | Start game from menu | Level scene created, gameplay begins |
| T9.5 | Press Escape during gameplay | Returns to main menu, level scene unloaded |
| T9.6 | Press P during gameplay | Game pauses, level scene paused |
| T9.7 | Press P while paused | Game resumes |
| T9.8 | Restart via menu after game over | New level scene created, level reset |

## Building

```batch
cd Build
msbuild zenith_win64.sln /p:Configuration=vs2022_Debug_Win64_True /p:Platform=x64
cd ..\Games\Marble\Build\output\win64\vs2022_debug_win64_true
marble.exe
```
