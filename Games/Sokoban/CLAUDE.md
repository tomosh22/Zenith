# Sokoban Example Game

A classic box-pushing puzzle game demonstrating core Zenith engine features.

## Engine Features Demonstrated

| Feature | Engine Class | Usage |
|---------|--------------|-------|
| **Entity-Component System** | `Zenith_Entity`, `Zenith_Scene` | Entity creation, component attachment |
| **Game Components** | `Zenith_ComponentMetaRegistry` | Game logic via component lifecycle hooks (OnAwake, OnStart, OnUpdate) |
| **Prefab System** | `Zenith_Prefab::Instantiate()` (method on prefab object), `Zenith_Prefab` | Entity templates for tiles, boxes, player |
| **Input Handling** | `Zenith_Input` | Keyboard input polling |
| **UI System** | `Zenith_UIComponent`, `Zenith_UIText` | Text elements with anchoring |
| **Model Rendering** | `Zenith_ModelComponent`, `Flux_MeshGeometry` | 3D mesh rendering with materials |
| **Materials/Textures** | `Zenith_MaterialAsset` | Procedural single-color textures |
| **DataAsset System** | `Zenith_DataAsset` | Configuration with serialization |
| **Serialization** | `Zenith_DataStream` | Behavior state persistence |
| **Camera** | `Zenith_CameraComponent` | Orthographic top-down view |
| **Multi-Scene** | `Zenith_SceneSystem` | `LoadSceneByIndex(..., SCENE_LOAD_SINGLE)`, `LoadScene(..., SCENE_LOAD_ADDITIVE_WITHOUT_LOADING)`, `SetActiveScene()`, `UnloadScene()` |
| **UI Buttons** | `Zenith_UIButton` | Clickable/tappable menu buttons with `SetOnClick()` callback |

## File Structure

```
Games/Sokoban/
  CLAUDE.md                      # This documentation
  Sokoban.cpp                    # Project entry points, resources, graph builders (BuildGraph_Sokoban*)
  Components/
    Sokoban_GameComponent.h      # Systems coordinator (step animation, visuals, level gen seams)
    Sokoban_Config.h             # DataAsset for game configuration
    Sokoban_GridLogic.h          # Movement and puzzle logic (pure C++, the systems the graph calls)
    Sokoban_Rendering.h          # 3D visualization (entity creation/update)
    Sokoban_LevelGenerator.h     # Procedural level generation
    Sokoban_Solver.h             # BFS solver for level validation
    Sokoban_Animation.h          # Smooth grid-step animation for player/boxes, dust emitter driving
    Sokoban_GraphNodes.h         # Behaviour Graph node library (5 systems-seam nodes)
  Tests/
    Test_SokobanCharacterization.cpp  # 4 automated tests (menu, move flow, reset, escape)
  Assets/
    Scenes/MainMenu.zscen         # Main menu scene (build index 0)
    Scenes/Sokoban.zscen          # Gameplay scene (build index 1)
    Graphs/Sokoban_LevelFlow.bgraph   # Level flow (owns the game state)
    Graphs/Sokoban_GameFlow.bgraph    # Menu flow (Play click + focus)
```

W2 conversion note: `Sokoban_Input.h` and `Sokoban_UIManager.h` are DELETED —
key→move dispatch is `OnKeyPressed` graph chains, and the HUD text writes are
engine `SetUIText` nodes (the one composite "Boxes: X / Y" string is staged by
`SokobanStageBoardFacts`).

## Behaviour Graphs (W2 — the first zero → full conversion)

Both graphs are boot-authored through `Zenith_GraphBuilder`
(`AddStep_GraphBuild`, `BuildGraph_Sokoban*` in Sokoban.cpp) and attached with
`AddStep_AttachGraph`. The game state LIVES on the graph blackboard:
`GetGameState`/`GetMoveCount`/`IsWon` read the attached graph's blackboard
(`gameState`/`moveCount`/`won`); grid facts (player/boxes/targets/minMoves)
stay C++. The component keeps SYSTEMS only (step-animation tween, visuals,
generator+solver, GridLogic move mechanics).

**`Sokoban_LevelFlow.bgraph`** (gameplay GameManager; `gameState`=PLAYING,
`moveCount`=0, `won`=false). The plan's separate "MoveFlow" lives as the move
section of this graph — move and level chains share `moveCount`/`won`, and
graph slots have separate blackboards, so one gameplay graph owns them (the
same shape Marble/Runner shipped). Anchors, mirroring the old OnUpdate order
(Esc before R before movement):
- `OnStart` → 10× `SetUIVisible(true)` → fire "SokobanRefreshHUD".
- `OnKeyPressed(Esc)` → `SokobanUnloadLevel` → `LoadSceneByIndex(0)`.
- `OnKeyPressed(R)` → `SokobanRegenerateLevel` → reset moveCount/won →
  fire "SokobanRefreshHUD".
- 8× `OnKeyPressed(W/A/S/D/arrows)` → `SokobanTryMove(direction)` (gates on
  won + mid-step; runs the GridLogic move/push + starts the step animation;
  FAILURE = blocked) → `AddBlackboardInt`(moveCount+1) → fire refresh.
- `OnCustomEvent("SokobanStepComplete")` (fired by the component the frame a
  step animation finishes) → `SokobanStageBoardFacts` →
  `CompareBlackboardInt`(targetCount>0) → `Branch` →
  `CompareBlackboardInt`(boxesOnTargets == targetCount, var-vs-var) →
  `Branch` → `SetBlackboardBool`(won) → fire refresh — the old
  `CheckWinCondition` as engine nodes.
- `OnCustomEvent("SokobanRefreshHUD")` → `SokobanStageBoardFacts` →
  `SetUIText`(Status "Moves: {}") → `SetUIText`(Progress "{}"=progressText) →
  `SetUIText`(MinMoves "Min Moves: {}") → `Branch`(won) →
  `SetUIText`(WinText "LEVEL COMPLETE!" / "") — the old Sokoban_UIManager.
  The graph fires this event at itself (`FireCustomEvent` node) from the
  move/win/R/OnStart chains — the chain-reuse pattern.

**`Sokoban_GameFlow.bgraph`** (menu MenuManager; `gameState`=MAIN_MENU):
`OnUIButtonClicked("MenuPlay")` → `LoadSceneByIndex(1)` (engine UIButton
trampoline — no C++ click wiring); `OnUpdate` → `SokobanFocusPlayButton`.

### Game node library (`Components/Sokoban_GraphNodes.h`, 5 systems seams)

| Node | What it does |
|---|---|
| `SokobanTryMove` | Move gate (won/mid-step) + GridLogic move/push + step-animation start; FAILURE = refused |
| `SokobanStageBoardFacts` | Publishes boxesOnTargets/targetCount/minMoves + the "Boxes: X / Y" progress string |
| `SokobanRegenerateLevel` | Fresh puzzle scene + generator + solver + visuals |
| `SokobanUnloadLevel` | Cancel step animation + clear renderer IDs + unload puzzle scene |
| `SokobanFocusPlayButton` | Keeps the single menu button focused |

Registered via `Sokoban_RegisterGraphNodes()` from
`Project_RegisterGameComponents`.

**Equivalence proof:** `Tests/Test_SokobanCharacterization.cpp` (headless OK;
written green against the C++ decisions FIRST, passing unchanged against the
graphs): `Sokoban_MenuPlay_Test` (Play focus + activation → PLAYING),
`Sokoban_MoveFlow_Test` (on the `Test_LoadFixtureLevel` corridor: blocked move
does nothing and doesn't count, neutral move counts, push-onto-target wins on
step completion, exact HUD strings, input dead after win),
`Sokoban_ResetFlow_Test` (R → fresh solver-validated level, counters reset),
`Sokoban_EscapeMenu_Test` (Esc → menu).

```
sokoban.exe --all-automated-tests --headless --exit-after-frames 45000 --fixed-dt 0.01666 --skip-unit-tests
```

> `--skip-unit-tests` is required: sokoban.exe units-at-boot trip the known
> layout-sensitive latent corruption (same family as test.exe — see the
> adoption-program notes); DP units are the reliable units gate.

**Engine fix found by this wave:** Sokoban's Esc path (unloading the puzzle
scene mid-update from gameplay code) exposed a use-after-free in
`Zenith_SceneSystem::Update` — the update loop snapshotted raw `SceneData*`
pointers while `UnloadScene` deletes synchronously. The snapshot is
handle+generation now (pinned by
`GraphComponent::UnloadSceneMidUpdateIsSafe`). Marble had dodged it by
accident (its Esc only fires from PAUSED; paused scenes are excluded from the
snapshot) and Runner had been silently reading freed memory.

## Module Breakdown

### Sokoban.cpp - Entry Points
**Engine APIs:** `Project_GetName`, `Project_RegisterGameComponents`, `Project_LoadInitialScene` (primary entry points); also `Project_GetGameAssetsDirectory`, `Project_SetGraphicsOptions`, `Project_Shutdown`

Demonstrates:
- Project lifecycle hooks
- Procedural geometry creation (`Zenith_MeshGeometryAsset::CreateUnitCube`)
- Runtime texture creation for materials
- Prefab creation for runtime instantiation
- Scene setup with camera and UI entities

### Sokoban_GameComponent.h - Main Coordinator
**Engine APIs:** `Zenith_ComponentMetaRegistry` (registered as "SokobanGame", order 100), lifecycle hooks

Demonstrates:
- `OnAwake()` - Runtime initialization (not called during scene load)
- `OnStart()` - Called before first update, after all OnAwake
- `OnUpdate(float fDt)` - Main game loop
- `RenderPropertiesPanel()` - Editor UI (tools build only)
- `WriteToDataStream/ReadFromDataStream` - Serialization (leading component version + parameter payload)

### Sokoban_GridLogic.h - Game Logic
**Engine APIs:** None (pure logic)

Demonstrates:
- Separating game logic from engine integration
- State management patterns
- Direction-based movement calculations

### Sokoban_Rendering.h - 3D Visualization
**Engine APIs:** `Zenith_TransformComponent`, `Zenith_ModelComponent`, `Zenith_Scene`

Demonstrates:
- Creating entities from prefabs
- Setting transform position/scale
- Adding model components with mesh/material
- Dynamic entity creation/destruction
- Coordinate space conversion (grid to world)

### Sokoban_LevelGenerator.h - Procedural Generation
**Engine APIs:** None (uses `<random>`)

Demonstrates:
- Procedural content generation patterns
- Random number generation with `std::mt19937`
- Level validation (checks solvability)

### Sokoban_Solver.h - BFS Solver
**Engine APIs:** None (pure algorithm)

Demonstrates:
- Breadth-first search implementation
- State space exploration with cycle detection
- Custom hash functions for complex state types
- Performance limiting (max states to explore)

### Sokoban_Animation.h - Grid-Step Animation
**Engine APIs:** `Zenith_ParticleEmitterComponent`

Demonstrates:
- Smooth grid-step interpolation for player movement and box pushes
- Driving a dust-trail particle emitter (the DustEmitter entity in the Sokoban scene) during a step
- Resolving an emitter by entity ID across scenes

## Multi-Scene Architecture

### Entity Layout
- **Sokoban scene** (build index 1): GameManager entity (Camera + UI + SokobanGame component) plus a separate DustEmitter entity (its own `ParticleEmitterComponent`). Loaded with `SCENE_LOAD_SINGLE` when Play is clicked (the MainMenu scene at index 0 is loaded first, then replaced).
- **Puzzle scene** (`m_xPuzzleScene`): Floor tiles, walls, boxes, targets, player - loaded additively (`SCENE_LOAD_ADDITIVE_WITHOUT_LOADING`) on top of the Sokoban scene and created/destroyed per level. Not pre-registered as a build index; created dynamically at runtime.

### Game State Machine
```
MAIN_MENU  -->  PLAYING  -->  MAIN_MENU
   ^                |              ^
   |                v (win + R)    |
   |           new level           |
   +---------- (Escape) ----------+
```

### Scene Transition Pattern
Clicking Play loads the Sokoban scene via `LoadSceneByIndex(1, SCENE_LOAD_SINGLE)`. From there, gameplay uses `LoadScene("Puzzle", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING)` + `SetActiveScene()` to start a level, `UnloadScene()` + `LoadScene()` to reset levels, and `LoadSceneByIndex(0, SCENE_LOAD_SINGLE)` to return to the MainMenu scene (the puzzle scene is unloaded first during cleanup). The `m_xPuzzleScene` handle is invalidated after unloading.

## Learning Path

1. **Start here:** `Sokoban.cpp` - See how projects initialize resources
2. **Understand game components:** `Sokoban_GameComponent.h` - Learn lifecycle hooks
3. **Study input:** `Sokoban_Input.h` - Simple input polling pattern
4. **See rendering:** `Sokoban_Rendering.h` - Entity/component creation
5. **Explore UI:** `Sokoban_UIManager.h` - Text element updates
6. **Advanced:** `Sokoban_Solver.h` - Algorithm implementation example

## Controls

| Key | Action |
|-----|--------|
| W / Up Arrow | Move up |
| S / Down Arrow | Move down |
| A / Left Arrow | Move left |
| D / Right Arrow | Move right |
| R | Reset/regenerate level |
| Click / Touch | Select menu button |
| W/S or Up/Down | Navigate menu |
| Enter | Activate focused button |
| Escape | Return to menu |

## Key Patterns

### Prefab Instantiation
Create entity from prefab via `Zenith_Prefab::Instantiate()` (called on the prefab object), set transform position, then add components (model, collider, etc.).

### Component Lifecycle
`OnAwake()` = runtime creation only, `OnStart()` = before first update, `OnUpdate(float fDt)` = every frame.

### UI Text Updates
Find UI elements via `xUI.FindElement<Zenith_UIText>("Name")` and call `SetText()` to update.

## Editor View (What You See on Boot)

When launching in a tools build (`vs2022_Debug_Win64_True`):

### Scene Hierarchy
- **GameManager** - Entity in the Sokoban scene (Camera + UI + SokobanGame component)
- **DustEmitter** - Separate entity in the same scene with its own `ParticleEmitterComponent`

### Viewport
- **Top-down orthographic view** of a procedurally generated puzzle grid
- **Gray floor tiles** filling the puzzle area
- **Brown/tan wall blocks** defining the puzzle boundaries
- **Orange/tan box entities** that can be pushed by the player
- **Green target positions** where boxes need to be placed
- **Blue player entity** (cube shape) at the starting position

### Properties Panel (when GameManager selected)
- **Grid size** - Puzzle dimensions
- **Box count** - Number of boxes/targets in puzzle
- **Move speed** - Player movement animation speed

### Console Output on Boot
```
[Sokoban] Generating level with seed: <random_seed>
[Sokoban] Level validated as solvable
[Sokoban] Created X floor entities, Y wall entities, Z box entities
```

## Gameplay View (What You See When Playing)

### Initial State
- Player (blue cube) positioned at starting location
- 2-5 boxes (orange/tan cubes) scattered on the grid
- Equal number of target positions (green markers on floor)
- Walls forming a contained puzzle area

### HUD Elements (Top-Right)
- **"Moves: 0"** - Move counter
- **"Boxes: 0 / 3"** - Boxes on targets counter
- **"Min Moves: 0"** - Minimum moves needed to solve (from BFS solver)

### Gameplay Actions
1. **Movement**: Player slides smoothly to adjacent tile
2. **Pushing**: When moving into a box, box slides to next tile (if not blocked)
3. **Win State**: All boxes on targets - "Level Complete!" message appears
4. **Reset**: R key regenerates a new random puzzle

### Visual Feedback
- Boxes change color slightly when on target positions
- Player movement is grid-locked (no diagonal movement)
- Invalid moves produce no action

## Test Plan

### T1: Boot and Initialization
| Step | Action | Expected Result |
|------|--------|-----------------|
| T1.1 | Launch sokoban.exe | Window opens with orthographic view of puzzle |
| T1.2 | Check console output | Level generation logs appear without errors |
| T1.3 | Verify entity count | GameManager entity visible in hierarchy |
| T1.4 | Check HUD | Moves, Boxes, and Min Moves counters display at 0 |

### T2: Menu Navigation
| Step | Action | Expected Result |
|------|--------|-----------------|
| T2.1 | Click Play button | Game transitions to PLAYING state, puzzle appears |
| T2.2 | Press W/S or Up/Down on menu | Button focus cycles (single button stays focused) |
| T2.3 | Press Enter on focused button | Game starts, menu hides |
| T2.4 | Press Escape during gameplay | Returns to main menu, puzzle scene unloads |
| T2.5 | Click Play again after returning | New puzzle scene created, game resumes |

### T3: Scene Transitions
| Step | Action | Expected Result |
|------|--------|-----------------|
| T3.1 | Start game from menu | Puzzle scene created, level entities spawned |
| T3.2 | Press Escape to return to menu | Puzzle scene unloaded, only GameManager remains |
| T3.3 | Complete level, press R | Old puzzle scene unloaded, new one created with fresh level |
| T3.4 | Rapid menu/game transitions | No crashes, scenes clean up properly |

### T4: Player Movement
| Step | Action | Expected Result |
|------|--------|-----------------|
| T4.1 | Press W or Up Arrow | Player moves up one tile |
| T4.2 | Press S or Down Arrow | Player moves down one tile |
| T4.3 | Press A or Left Arrow | Player moves left one tile |
| T4.4 | Press D or Right Arrow | Player moves right one tile |
| T4.5 | Move into wall | Player does not move, no crash |
| T4.6 | Check HUD after move | Moves counter increments by 1 |

### T5: Box Pushing
| Step | Action | Expected Result |
|------|--------|-----------------|
| T5.1 | Push box into empty space | Box moves, player moves into box's old position |
| T5.2 | Push box into wall | Neither box nor player moves |
| T5.3 | Push box into another box | Neither box nor player moves |
| T5.4 | Push box onto target | Box placed, "Boxes: X / Y" counter updates |

### T6: Win Condition
| Step | Action | Expected Result |
|------|--------|-----------------|
| T6.1 | Place all boxes on targets | "Level Complete!" message appears |
| T6.2 | Press R after win | New level generates |
| T6.3 | Verify new level | Different puzzle layout from previous |

### T7: Level Reset
| Step | Action | Expected Result |
|------|--------|-----------------|
| T7.1 | Make several moves | Moves counter > 0 |
| T7.2 | Press R | New level generates |
| T7.3 | Check counters | All counters reset to 0 |
| T7.4 | Verify solvability | Level can be completed (uses solver validation) |

### T8: Edge Cases
| Step | Action | Expected Result |
|------|--------|-----------------|
| T8.1 | Rapid key presses | Movement queues properly, no stuck state |
| T8.2 | Hold movement key | Single move per key press (not continuous) |
| T8.3 | Press multiple directions | Only one direction processed |
| T8.4 | Minimize/restore window | Game resumes correctly |

### T9: Editor Features (Tools Build Only)
| Step | Action | Expected Result |
|------|--------|-----------------|
| T9.1 | Select GameManager entity | Properties panel appears |
| T9.2 | Modify configuration values | Changes apply to next level |
| T9.3 | Save scene | Sokoban.zscen updates in Assets/Scenes |

## Building

```batch
cd Build
msbuild zenith_win64.sln /p:Configuration=vs2022_Debug_Win64_True /p:Platform=x64
cd ..\Games\Sokoban\Build\output\win64\vs2022_debug_win64_true
sokoban.exe
```
