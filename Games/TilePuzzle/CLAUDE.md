# TilePuzzle Example Game

A cat-matching sliding puzzle game demonstrating the Zenith entity-component system and multi-scene management.

## Overview

Players slide colored shapes across a grid to match them with cats of the same color. When a shape overlaps a matching cat, the cat is eliminated. The goal is to eliminate all cats with minimal moves.

## Files

- `TilePuzzle.cpp` - Main entry point, resource initialization, scene setup
- `Components/TilePuzzle_Behaviour.h` - Main game coordinator (implements `Zenith_ScriptBehaviour`)
- `Components/TilePuzzle_Types.h` - Game state enums, level data structures
- `Components/TilePuzzle_LevelGenerator.h` - Procedural level generation
- `Components/TilePuzzle_Solver.h` - Level solvability verification

## Engine Features Demonstrated

| Feature | Engine Class | Usage |
|---------|--------------|-------|
| **Script Behaviours** | `Zenith_ScriptBehaviour` | Main game logic in `TilePuzzle_Behaviour` |
| **Scene Management** | `Zenith_SceneManager` | Get active scene, create entities |
| **Entity Creation** | `Zenith_Entity` | Floor tiles, shapes, cats |
| **Prefab System** | `Zenith_Prefab` | Template-based entity instantiation |
| **Components** | `Zenith_TransformComponent`, `Zenith_ModelComponent`, `Zenith_UIComponent` | Position, rendering, UI |
| **Input System** | `Zenith_Input` | Keyboard controls |
| **Multi-Scene** | `Zenith_SceneManager` | `DontDestroyOnLoad()`, `CreateEmptyScene()`, `UnloadScene()` |
| **UI Buttons** | `Zenith_UIButton` | Clickable/tappable menu buttons with `SetOnClick()` callback |
| **Timed Destruction** | `Zenith_SceneManager::Destroy(entity, delay)` | Cat elimination with 0.3s delay |

## Game Architecture

### State Machine
```
MAIN_MENU -> GENERATING -> PLAYING -> SHAPE_SLIDING -> CHECK_ELIMINATION -> LEVEL_COMPLETE
   ^                         ^              |                   |                |
   |                         |              v                   |                |
   |                         +--------------+-------------------+                |
   +--- (Escape) -----------+                                                   |
                             +--- (N for next level) ---------------------------+
```

### Level Data
```cpp
TilePuzzleLevelData:
  - uGridWidth, uGridHeight: Grid dimensions
  - aeCells: 2D array of cell types (FLOOR, EMPTY)
  - axShapes: Vector of shape instances with positions/colors
  - axCats: Vector of cats with positions/colors/elimination state
```

### Entity Hierarchy
```
GameManager (Camera + UI + Script) - DontDestroyOnLoad
Puzzle scene (created/destroyed per level):
  Floor entities (from prefab)
  Shape cube entities (from prefab)
  Cat sphere entities (from prefab)
```

## Module Breakdown

### TilePuzzle.cpp - Entry Points
**Engine APIs:** `Project_GetName`, `Project_RegisterScriptBehaviours`, `Project_CreateScenes`, `Project_LoadInitialScene`

Demonstrates:
- Project lifecycle hooks
- Procedural geometry creation (cube + sphere)
- Runtime texture/material creation
- Prefab creation for runtime instantiation
- Scene setup with persistent GameManager entity

### TilePuzzle_Behaviour.h - Main Coordinator
**Engine APIs:** `Zenith_ScriptBehaviour`, `Zenith_SceneManager`, `Zenith_UIButton`

Demonstrates:
- Multi-scene architecture with persistent and puzzle scenes
- Menu system with button callbacks via `SetOnClick()`
- Scene transitions (create/unload puzzle scenes)
- Timed entity destruction for cat elimination
- Selection highlighting via material swaps

## Multi-Scene Architecture

### Entity Layout
- **Persistent scene**: GameManager entity (Camera + UI + Script), marked with `DontDestroyOnLoad()`
- **Puzzle scene** (`m_xPuzzleScene`): Floor tiles, shape cubes, cat spheres - created/destroyed per level

### Game State Machine
```
MAIN_MENU  -->  PLAYING  -->  MAIN_MENU
   ^                |              ^
   |                v (N)          |
   |           next level          |
   +---------- (Escape) ----------+
```

### Scene Transition Pattern
Uses `CreateEmptyScene("Puzzle")` + `SetActiveScene()` to start, `UnloadScene()` + `CreateEmptyScene()` to load next level, and `UnloadScene()` to return to menu. Handle is invalidated after unloading.

## Controls

| Key | Action |
|-----|--------|
| WASD / Arrows | Move cursor / selected shape |
| Space | Select/deselect shape at cursor |
| R | Reset level |
| N | Next level (when complete) |
| Click / Touch | Select menu button |
| W/S or Up/Down | Navigate menu |
| Enter | Activate focused button |
| Escape | Return to menu |

## Scene Management Usage

Get active scene via `GetActiveScene()`, instantiate entities from prefabs via `Prefab::Instantiate()`, destroy entities immediately with `Destroy(entity)` or with delay via `Destroy(entity, 0.3f)` for cat elimination effects.

## Behaviour Registration

Behaviours must be registered in `Project_RegisterScriptBehaviours()` via `TilePuzzle_Behaviour::RegisterBehaviour()` before scene deserialization.

## Editor View (What You See on Boot)

When launching in a tools build (`vs2022_Debug_Win64_True`):

### Scene Hierarchy
- **GameManager** - Persistent entity (Camera + UI + Script) - `DontDestroyOnLoad`

### Properties Panel (when GameManager selected)
- **Level** - Current level number
- **Moves** - Move counter
- **Cats remaining** - Number of uneliminated cats
- **State** - Current game state

## Test Plan

### T1: Boot and Initialization
| Step | Action | Expected Result |
|------|--------|-----------------|
| T1.1 | Launch tilepuzzle.exe | Window opens with main menu |
| T1.2 | Check console output | No errors in log |
| T1.3 | Verify entity count | GameManager entity visible in hierarchy |

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
| T3.3 | Complete level, press N | Old puzzle scene unloaded, new one created with next level |
| T3.4 | Rapid menu/game transitions | No crashes, scenes clean up properly |

### T4: Shape Movement
| Step | Action | Expected Result |
|------|--------|-----------------|
| T4.1 | Move cursor with WASD/Arrows | Cursor highlights floor tile |
| T4.2 | Press Space on shape | Shape selected (highlighted) |
| T4.3 | Move selected shape | Shape slides in direction |
| T4.4 | Move shape into wall/blocker | Shape does not move |
| T4.5 | Press Space again | Shape deselected |

### T5: Cat Elimination
| Step | Action | Expected Result |
|------|--------|-----------------|
| T5.1 | Slide shape onto matching cat | Cat eliminated after 0.3s delay |
| T5.2 | Slide shape onto non-matching cat | No elimination |
| T5.3 | Eliminate all cats | "LEVEL COMPLETE!" message appears |

### T6: Level Progression
| Step | Action | Expected Result |
|------|--------|-----------------|
| T6.1 | Complete level, press N | Next level loads with new layout |
| T6.2 | Press R during gameplay | Level resets with same configuration |

### T7: Edge Cases
| Step | Action | Expected Result |
|------|--------|-----------------|
| T7.1 | Rapid key presses | No stuck state or double-moves |
| T7.2 | Press Escape from level complete | Returns to menu cleanly |
| T7.3 | Minimize/restore window | Game resumes correctly |

## Future Enhancements

- [ ] Async loading with progress bar
- [ ] Level selection screen
