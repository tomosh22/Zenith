# Sokoban Example Game

A classic box-pushing puzzle game demonstrating core Zenith engine features.

## Engine Features Demonstrated

| Feature | Engine Class | Usage |
|---------|--------------|-------|
| **Entity-Component System** | `Zenith_Entity`, `Zenith_Scene` | Entity creation, component attachment |
| **Script Behaviors** | `Zenith_ScriptBehaviour` | Game logic via lifecycle hooks (OnAwake, OnStart, OnUpdate) |
| **Prefab System** | `Zenith_Prefab`, `Zenith_Scene::Instantiate` | Entity templates for tiles, boxes, player |
| **Input Handling** | `Zenith_Input` | Keyboard input polling |
| **UI System** | `Zenith_UIComponent`, `Zenith_UIText` | Text elements with anchoring |
| **Model Rendering** | `Zenith_ModelComponent`, `Flux_MeshGeometry` | 3D mesh rendering with materials |
| **Materials/Textures** | `Zenith_MaterialAsset` | Procedural single-color textures |
| **DataAsset System** | `Zenith_DataAsset` | Configuration with serialization |
| **Serialization** | `Zenith_DataStream` | Behavior state persistence |
| **Camera** | `Zenith_CameraComponent` | Orthographic top-down view |
| **Multi-Scene** | `Zenith_SceneManager` | `DontDestroyOnLoad()`, `CreateEmptyScene()`, `UnloadScene()` |
| **UI Buttons** | `Zenith_UIButton` | Clickable/tappable menu buttons with `SetOnClick()` callback |

## File Structure

```
Games/Sokoban/
  CLAUDE.md                      # This documentation
  Sokoban.cpp                    # Project entry points, resource initialization
  Components/
    Sokoban_Behaviour.h          # Main coordinator (uses modules below)
    Sokoban_Config.h             # DataAsset for game configuration
    Sokoban_Input.h              # Keyboard input handling
    Sokoban_GridLogic.h          # Movement and puzzle logic
    Sokoban_Rendering.h          # 3D visualization (entity creation/update)
    Sokoban_LevelGenerator.h     # Procedural level generation
    Sokoban_Solver.h             # BFS solver for level validation
    Sokoban_UIManager.h          # HUD text management
  Assets/
    Scenes/Sokoban.zscn          # Serialized scene
```

## Module Breakdown

### Sokoban.cpp - Entry Points
**Engine APIs:** `Project_Initialise`, `Project_RegisterScriptBehaviours`, `Project_CreateScene`

Demonstrates:
- Project lifecycle hooks
- Procedural geometry creation (`Flux_MeshGeometry::GenerateUnitCube`)
- Runtime texture creation for materials
- Prefab creation for runtime instantiation
- Scene setup with camera and UI entities

### Sokoban_Behaviour.h - Main Coordinator
**Engine APIs:** `Zenith_ScriptBehaviour`, lifecycle hooks

Demonstrates:
- `OnAwake()` - Runtime initialization (not called during scene load)
- `OnStart()` - Called before first update, after all OnAwake
- `OnUpdate(float fDt)` - Main game loop
- `RenderPropertiesPanel()` - Editor UI (tools build only)
- `WriteParametersToDataStream/ReadParametersFromDataStream` - Serialization

### Sokoban_Input.h - Input Handling
**Engine APIs:** `Zenith_Input::WasKeyPressedThisFrame`

Demonstrates:
- Polling keyboard state
- Key code constants (`ZENITH_KEY_W`, `ZENITH_KEY_UP`, etc.)
- Frame-based input (pressed this frame vs held)

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

### Sokoban_UIManager.h - UI Management
**Engine APIs:** `Zenith_UIComponent`, `Zenith_UIText`

Demonstrates:
- Finding UI elements by name
- Dynamic text updates
- UI anchor/pivot system (TopRight positioning)

## Multi-Scene Architecture

### Entity Layout
- **Persistent scene**: GameManager entity (Camera + UI + Script + ParticleEmitter), marked with `DontDestroyOnLoad()`
- **Puzzle scene** (`m_xPuzzleScene`): Floor tiles, walls, boxes, targets, player - created/destroyed per level

### Game State Machine
```
MAIN_MENU  -->  PLAYING  -->  MAIN_MENU
   ^                |              ^
   |                v (win + R)    |
   |           new level           |
   +---------- (Escape) ----------+
```

### Scene Transition Pattern
```cpp
// Starting a game from menu
void StartGame()
{
    m_xPuzzleScene = Zenith_SceneManager::CreateEmptyScene("Puzzle");
    Zenith_SceneManager::SetActiveScene(m_xPuzzleScene);
    GenerateNewLevel();
}

// Loading a new level (resets puzzle scene)
void StartNewLevel()
{
    Zenith_SceneManager::UnloadScene(m_xPuzzleScene);
    m_xPuzzleScene = Zenith_SceneManager::CreateEmptyScene("Puzzle");
    Zenith_SceneManager::SetActiveScene(m_xPuzzleScene);
    GenerateNewLevel();
}

// Returning to menu
void ReturnToMenu()
{
    Zenith_SceneManager::UnloadScene(m_xPuzzleScene);
    m_xPuzzleScene = Zenith_Scene();  // invalidate handle
    m_eState = SokobanGameState::MAIN_MENU;
}
```

## Learning Path

1. **Start here:** `Sokoban.cpp` - See how projects initialize resources
2. **Understand behaviors:** `Sokoban_Behaviour.h` - Learn lifecycle hooks
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
```cpp
// Create entity from prefab template
Zenith_Entity xEntity = Zenith_Scene::Instantiate(*pxPrefab, "EntityName");

// Set transform BEFORE adding components (important for physics)
Zenith_TransformComponent& xTransform = xEntity.GetComponent<Zenith_TransformComponent>();
xTransform.SetPosition(xPos);

// Add components after transform is set
Zenith_ModelComponent& xModel = xEntity.AddComponent<Zenith_ModelComponent>();
xModel.AddMeshEntry(*pxMesh, *pxMaterial);
```

### Behavior Lifecycle
```cpp
void OnAwake()   // Called at RUNTIME creation only (not scene load)
void OnStart()   // Called before first OnUpdate, for ALL entities
void OnUpdate(float fDt)  // Called every frame
```

### UI Text Updates
```cpp
Zenith_UIComponent& xUI = entity.GetComponent<Zenith_UIComponent>();
Zenith_UI::Zenith_UIText* pxText = xUI.FindElement<Zenith_UI::Zenith_UIText>("ElementName");
if (pxText) {
    pxText->SetText("New text content");
}
```

## Editor View (What You See on Boot)

When launching in a tools build (`vs2022_Debug_Win64_True`):

### Scene Hierarchy
- **GameManager** - Persistent entity (Camera + UI + Script + ParticleEmitter) - `DontDestroyOnLoad`

### Viewport
- **Top-down orthographic view** of a procedurally generated puzzle grid
- **Gray floor tiles** filling the puzzle area
- **Brown/tan wall blocks** defining the puzzle boundaries
- **Blue box entities** that can be pushed by the player
- **Green target positions** where boxes need to be placed
- **Red player entity** (cube shape) at the starting position

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
- Player (red cube) positioned at starting location
- 3-5 boxes (blue cubes) scattered on the grid
- Equal number of target positions (green markers on floor)
- Walls forming a contained puzzle area

### HUD Elements (Top-Right)
- **"Moves: 0"** - Move counter
- **"Pushes: 0"** - Push counter
- **"Boxes: 0/3"** - Boxes on targets counter

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
| T1.4 | Check HUD | Moves, Pushes, and Boxes counters display at 0 |

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
| T5.4 | Push box onto target | Box placed, "Boxes: X/Y" counter updates |
| T5.5 | Check HUD after push | Pushes counter increments by 1 |

### T6: Win Condition
| Step | Action | Expected Result |
|------|--------|-----------------|
| T6.1 | Place all boxes on targets | "Level Complete!" message appears |
| T6.2 | Press any move key after win | New level generates |
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
| T9.3 | Save scene | Sokoban.zscn updates in Assets/Scenes |

## Building

```batch
cd Build
msbuild zenith_win64.sln /p:Configuration=vs2022_Debug_Win64_True /p:Platform=x64
cd ..\Games\Sokoban\Build\output\win64\vs2022_debug_win64_true
sokoban.exe
```
