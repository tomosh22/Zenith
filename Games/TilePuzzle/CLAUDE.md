# TilePuzzle Example Game

A cat-matching sliding puzzle game demonstrating the Zenith entity-component system and multi-scene management.

## Overview

Players slide colored polyomino shapes across a grid to match them with cats of the same color. When a shape overlaps a matching cat, the cat is eliminated. The goal is to eliminate all cats with minimal moves.

## Files

- `TilePuzzle.cpp` - Main entry point, resource initialization, scene setup
- `Components/TilePuzzle_Behaviour.h` - Main game coordinator (implements `Zenith_ScriptBehaviour`)
- `Components/TilePuzzle_Types.h` - Game state enums, shape definitions, level data structures
- `Components/TilePuzzle_LevelGenerator.h` - Procedural level generation via reverse scramble
- `Components/TilePuzzle_Rules.h` - Shared game rules (single source of truth for solver and gameplay)
- `Components/TilePuzzle_Solver.h` - BFS level solver (standalone, not used by generator)
- `Components/TilePuzzle_SaveData.h` - Save/load system with per-level records

## Game Rules

### Movement
Shapes move one cell at a time in four cardinal directions (up, down, left, right). A move is valid only if every cell of the shape would land on a valid floor tile within grid bounds.

### Collision
A shape is blocked by:
- Grid boundaries and empty (non-floor) cells
- Static blockers (non-draggable shapes)
- Other draggable shapes
- Cats of a **different** color (non-eliminated)

A shape is **not** blocked by:
- Cats of the **same** color (the cat will be eliminated after the move)
- Already-eliminated cats

### Elimination
When a same-color shape overlaps a cat's position, the cat is eliminated. Elimination is permanent within a level. The cat entity is destroyed with a 0.3s delay for visual feedback.

### Blocker-Cats
A cat can sit on top of a static blocker cell (`bOnBlocker = true`). Since shapes cannot enter blocker cells, these cats are eliminated by **orthogonal adjacency** instead of overlap: when a same-color shape cell occupies any of the four cardinal neighbors of the blocker-cat, the cat is eliminated.

### Conditional Shapes
A shape with `uUnlockThreshold > 0` is locked until at least that many cats have been eliminated. Locked shapes cannot be moved. A yellow number indicator floats above locked shapes showing how many more eliminations are needed. Once enough cats are eliminated, the indicator disappears and the shape becomes movable.

### Win Condition
A level is complete when all cats have been eliminated. Tracked via a bitmask (`uEliminatedCatsMask`) where each bit represents one cat.

## Level Generation (Reverse Scramble)

Levels are generated using a **reverse scramble** algorithm that guarantees solvability by construction, without needing a BFS solver.

### Algorithm Summary
1. Start from a **solved configuration** where each shape sits on its matching cat
2. **Scramble** by making random valid moves, using the same movement rules as gameplay
3. The reverse of the scramble sequence is a valid solution

### Generation Phases

**Phase 1 - Grid + Blockers**: Create a grid with an empty border. Place static single-cell blockers on random interior floor cells.

**Phase 2 - Cats**: Place normal cats on unoccupied floor cells, one or more per color based on difficulty. Then place blocker-cats on existing blocker positions (these share a cell with a static blocker and are marked `bOnBlocker = true`).

**Phase 3 - Shapes**: For each normal cat's color, place a draggable shape so one of its cells overlaps the cat. Shape type (single, domino, L, T, etc.) is chosen randomly based on difficulty. Falls back to single-cell if the chosen shape doesn't fit. For each blocker-cat, place a single-cell shape on an orthogonally adjacent floor cell (the solved position for adjacency-based elimination). Finally, mark shapes as conditional based on difficulty settings (`uUnlockThreshold`).

**Phase 4 - Scramble**: First, a pre-scramble phase moves conditional shapes off their cats while the covered mask is still high (ensuring they can actually move). Then the main scramble makes random valid moves:
- Pick a random draggable shape and a random direction
- Validate via `TilePuzzle_Rules::CanMoveShape`, passing the **covered mask** as the eliminated mask
- If valid, move the shape and recompute the covered mask

### Covered Mask vs Eliminated Mask
- **Eliminated mask** (gameplay): permanent, bits only set, never cleared. Once a cat is eliminated it stays eliminated.
- **Covered mask** (scramble): recomputed from scratch after each move. A cat is "covered" when a same-color shape currently overlaps it. Bits can be set or cleared as shapes move around.

The scramble passes the covered mask to `CanMoveShape` in place of the eliminated mask. This means covered cats don't block movement (same as eliminated cats in gameplay), maintaining rule consistency.

### Termination Conditions
The scramble loop runs until all three conditions are met:
1. `uEverCoveredMask == uAllCatsBits` - every cat was covered at some point during scramble (ensures the forward solution visits every cat)
2. `uCoveredMask == 0` - no cat is currently under a shape (all cats are visible to the player at level start)
3. `uSuccessfulMoves >= uScrambleMoves` - enough moves were made for the desired difficulty

### Why Solvability Is Guaranteed
Every scramble move is validated by `CanMoveShape`. The reverse of any valid move is also valid (moving back to the cell just vacated). Therefore, reversing the entire scramble sequence produces a valid solution that eliminates all cats.

## Difficulty System

Levels cycle through three tiers: **Normal** (level 1, 4, 7...), **Hard** (level 2, 5, 8...), **Very Hard** (level 3, 6, 9...) using `(levelNumber - 1) % 3`.

All difficulty parameters are `static constexpr` constants at the top of `TilePuzzle_LevelGenerator.h`, easily tunable:

| Parameter | Normal | Hard | Very Hard |
|-----------|--------|------|-----------|
| Grid size | 6x8 | 7x9 | 10x12 |
| Colors | 2 | 3 | 3 |
| Cats per color | 2 | 2 | 4 |
| Shapes per color | 2 | 2 | 2 |
| Blockers | 4 | 8 | 12 |
| Max shape size | 2 | 3 | 4 |
| Scramble moves | 15 | 30 | 60 |
| Blocker-cats | 0 | 2 | 2 |
| Conditional shapes | 0 | 0 | 1 |
| Conditional threshold | 0 | 0 | 2 |

The grid has a 1-cell empty border, so playable area is `(width-2) x (height-2)`.

## Shared Rules Architecture

`TilePuzzle_Rules.h` is the **single source of truth** for all gameplay logic. Both the game (`TilePuzzle_Behaviour`) and the level generator (scramble) use the same rule functions, ensuring they can never interpret rules differently.

### Lightweight State Views
Rules operate on `ShapeState` and `CatState` structs that contain only logical data (position, color, definition pointer) without entity/visual information. This allows the same rule functions to be called from gameplay code (which has entities) and the generator (which doesn't).

### Key Functions
- `CanMoveShape` - validates a proposed move against all collision rules (bounds, floor, blockers, shapes, cats). Also enforces conditional shape locking via `uUnlockThreshold`.
- `ComputeNewlyEliminatedCats` - returns a bitmask of newly eliminated cats. Handles both normal cats (overlap) and blocker-cats (orthogonal adjacency via `bOnBlocker`).
- `AreAllCatsEliminated` - checks if all cat bits are set in the elimination mask

## Shape Definitions

Shapes are defined as templates (`TilePuzzleShapeDefinition`) containing a list of relative cell offsets from an origin point and a draggable flag. Predefined shapes are available in the `TilePuzzleShapes` namespace: Single, Domino, L, T, I, S, Z, O.

Shape instances (`TilePuzzleShapeInstance`) store a pointer to their definition plus runtime position and color. The generator stores definitions in a static `Zenith_Vector` via `GetShapeDefinitions()`. Because shape instances hold raw pointers into this vector, `Reserve()` must be called before any `PushBack()` to prevent reallocation and dangling pointers.

## Save System

`TilePuzzle_SaveData.h` defines the save format:
- `uHighestLevelReached` - gates level select progression
- `uCurrentLevel` - resume point for Continue button
- `axLevelRecords[100]` - per-level best moves, best time, and completion flag

Auto-saves on level completion and when returning to menu. Uses `Zenith_SaveData::Save/Load` with static function pointer callbacks for serialization (no `std::function`).

## Extensibility

The reverse scramble architecture has two key extension points for adding new game object types:

### `TryScrambleMove` - Movement Patterns
Controls how shapes move during scramble. Conditional shapes are already implemented here (threshold check via `CanMoveShape`). Extend for:
- **Tandem polyominoes**: validate and move both paired shapes together

### `ComputeCoveredMask` - Elimination Mechanics
Controls what counts as "covering" a cat. Blocker-cats are already implemented here (adjacency check for `bOnBlocker` cats). Extend for new elimination mechanics beyond overlap and adjacency.

### `TilePuzzle_Rules::CanMoveShape` - Collision Rules
Add new collision types or movement constraints here. Both gameplay and scramble automatically pick up changes since they share this function.

The scramble loop itself never needs to change when adding new object types.

## Engine Features Demonstrated

| Feature | Engine Class | Usage |
|---------|--------------|-------|
| **Script Behaviours** | `Zenith_ScriptBehaviour` | Main game logic in `TilePuzzle_Behaviour` |
| **Scene Management** | `Zenith_SceneManager` | Get active scene, create entities |
| **Entity Creation** | `Zenith_Entity` | Floor tiles, shapes, cats |
| **Prefab System** | `Zenith_Prefab` | Template-based entity instantiation |
| **Components** | `Zenith_TransformComponent`, `Zenith_ModelComponent`, `Zenith_UIComponent` | Position, rendering, UI |
| **Input System** | `Zenith_Input` | Keyboard and mouse/touch controls |
| **Multi-Scene** | `Zenith_SceneManager` | `DontDestroyOnLoad()`, `CreateEmptyScene()`, `UnloadScene()` |
| **UI Buttons** | `Zenith_UIButton` | Clickable/tappable buttons with `SetOnClick()` callback |
| **Timed Destruction** | `Zenith_SceneManager::Destroy(entity, delay)` | Cat elimination with 0.3s delay |
| **Save System** | `Zenith_SaveData` | Persistent progress with static callbacks |

## Game Architecture

### State Machine
```
MAIN_MENU -> LEVEL_SELECT -> MAIN_MENU
MAIN_MENU -> PLAYING -> SHAPE_SLIDING -> CHECK_ELIMINATION -> LEVEL_COMPLETE
                ^              |                   |                |
                |              v                   |                |
                +--------------+-------------------+                |
  (Escape) --> MAIN_MENU                                           |
                             (N for next level) -------------------+
```

### Entity Hierarchy
```
GameManager (Camera + UI + Script) - persistent across scenes
Puzzle scene (created/destroyed per level):
  Floor entities (from prefab)
  Shape cube entities (from prefab)
  Cat sphere entities (from prefab)
```

### Multi-Scene Pattern
- **Scene 0 (MainMenu)**: Contains GameManager with menu UI and script behaviour
- **Scene 1 (TilePuzzle)**: Contains GameManager with gameplay HUD and script behaviour
- Puzzle scene (`m_xPuzzleScene`) is created/destroyed dynamically within either scene

## Controls

| Key | Action |
|-----|--------|
| WASD / Arrows | Move cursor / selected shape |
| Space | Select/deselect shape at cursor |
| Click + Drag | Drag shapes directly |
| R | Reset level |
| N | Next level (when complete) |
| Escape | Return to menu |

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
| T2.1 | Click Continue | Game transitions to PLAYING state, puzzle appears |
| T2.2 | Click Level Select | Level grid appears with page navigation |
| T2.3 | Click New Game | Save data resets, starts from level 1 |
| T2.4 | Press Escape during gameplay | Returns to main menu |

### T3: Level Generation
| Step | Action | Expected Result |
|------|--------|-----------------|
| T3.1 | Play level 1 (Normal) | 6x6 grid, 2 colors, level is solvable |
| T3.2 | Play level 2 (Hard) | 7x7 grid, 3 colors, level is solvable |
| T3.3 | Play level 3 (Very Hard) | 10x10 grid, 3 colors, 2 cats/color, level is solvable |
| T3.4 | Play levels 4-9 | Difficulty cycle repeats correctly |
| T3.5 | Generation speed | All levels generate near-instantly (no multi-second delays) |

### T4: Shape Movement
| Step | Action | Expected Result |
|------|--------|-----------------|
| T4.1 | Move cursor with WASD/Arrows | Cursor highlights floor tile |
| T4.2 | Press Space on shape | Shape selected (highlighted with emissive glow) |
| T4.3 | Move selected shape | Shape slides one cell in direction |
| T4.4 | Move shape into wall/blocker | Shape does not move |
| T4.5 | Click and drag shape | Shape follows cursor, snaps to grid |

### T5: Cat Elimination
| Step | Action | Expected Result |
|------|--------|-----------------|
| T5.1 | Slide shape onto matching cat | Cat eliminated after 0.3s delay |
| T5.2 | Slide shape onto non-matching cat | Shape is blocked, does not move |
| T5.3 | Eliminate all cats | "LEVEL COMPLETE!" message appears |

### T6: Save/Load
| Step | Action | Expected Result |
|------|--------|-----------------|
| T6.1 | Complete a level | Save data updated, next level unlocked |
| T6.2 | Return to menu, click Continue | Resumes from saved level |
| T6.3 | Check level select | Completed levels show star, locked levels are greyed out |
