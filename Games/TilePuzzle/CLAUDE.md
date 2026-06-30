# TilePuzzle Example Game

A cat-matching sliding puzzle game demonstrating the Zenith entity-component system and multi-scene management.

## Overview

Players slide colored polyomino shapes across a grid to match them with cats of the same color. When a shape overlaps a matching cat, the cat is eliminated. The goal is to eliminate all cats with minimal moves.

## Files

- `TilePuzzle.cpp` - Main entry point, resource initialization, scene setup
- `Components/TilePuzzle_GameComponent.h` - Main game coordinator (game ECS component)
- `Components/TilePuzzle_Types.h` - Game state enums, shape definitions, level data structures
- `Components/TilePuzzle_LevelGenerator.h` - Procedural level generation via reverse scramble
- `Components/TilePuzzle_Rules.h` - Shared game rules (single source of truth for solver and gameplay)
- `Components/TilePuzzle_Solver.h` - BFS level solver used for solvability verification and difficulty assessment during level generation
- `Components/TilePuzzle_ConditionalValidator.h` - Validates conditional shapes by re-solving with the threshold removed; drops conditions that don't change the minimum move count
- `Components/TilePuzzleLevelData_Serialize.h` - Binary `.tlvl` serialization for `TilePuzzleLevelData` (magic `TPLV`, version 2; shape definitions inlined)
- `Components/TilePuzzle_AssetGen.h` - SDF-based procedural texture/asset generation (`TilePuzzle_SDF` helpers)
- `Components/TilePuzzle_MetaGame.h` - Meta-game systems (cat cafe, victory overlay, coins, energy/lives, daily puzzle, star rendering) mixed into `TilePuzzle_GameComponent` via inclusion (not standalone)
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

`GetDifficultyForLevel(uLevelNumber)` in `TilePuzzle_LevelGenerator.h` maps the level number (1-100) onto a **7-tier progressive difficulty curve**. There is no `% 3` cycling — each tier covers a contiguous level range and most parameters are min-max ranges (the generator picks a value within the range per level).

| Tier | Levels | Grid (WxH) | Colors | Cats/color | Blockers | Blocker-cats | Max shape size | Conditional shapes (threshold) | Scramble moves |
|------|--------|-----------|--------|-----------|----------|--------------|----------------|--------------------------------|----------------|
| Tutorial Early | 1-5 | 5-6 x 5-6 | 1-2 | 1 | 0 | 0 | 1 (single-cell only) | 0 | 50 |
| Tutorial Late | 6-10 | 5-6 x 5-6 | 1-2 | 1 | 0 | 0 | 2 (multi-cell introduced) | 0 | 50 |
| Easy | 11-25 | 6-7 x 6-7 | 2 | 1-2 | 1-2 | 0 | 1-3 | 0 | 100 |
| Medium | 26-45 | 7-8 x 7-8 | 2-3 | 2 | 1-2 | 1 (introduced @26) | 2-3 | 0 | 200 |
| Hard | 46-65 | 8 x 8 | 3 | 2-3 | 1-2 | 1 | 3-4 | 1 (introduced @46), threshold 2 | 400 |
| Expert | 66-80 | 8-9 x 8-9 | 3-4 | 2-3 | 1-2 | 1 | 3-4 | 1, threshold 2 | 500 |
| Master | 81-100 | 8 x 8 | 3 | 3 | 1-2 | 1 | 3-4 | 1, threshold `s_uGenMaxConditionalThreshold` | `s_uGenScrambleMoves` |

`uNumColors` is clamped to `TILEPUZZLE_COLOR_COUNT` and `uMaxShapeSize` to 4. Several Master-tier values fall back to `static constexpr` `s_uGen*` constants at the top of `TilePuzzle_LevelGenerator.h`, which are easily tunable.

The grid has a 1-cell empty border, so playable area is `(width-2) x (height-2)`.

## Shared Rules Architecture

`TilePuzzle_Rules.h` is the **single source of truth** for all gameplay logic. Both the game (`TilePuzzle_GameComponent`) and the level generator (scramble) use the same rule functions, ensuring they can never interpret rules differently.

### Lightweight State Views
Rules operate on `ShapeState` and `CatState` structs that contain only logical data (position, color, definition pointer) without entity/visual information. This allows the same rule functions to be called from gameplay code (which has entities) and the generator (which doesn't).

### Key Functions
- `CanMoveShape` - validates a proposed move against all collision rules (bounds, floor, blockers, shapes, cats). Also enforces conditional shape locking via `uUnlockThreshold`.
- `ComputeNewlyEliminatedCats` - returns a bitmask of newly eliminated cats. Handles both normal cats (overlap) and blocker-cats (orthogonal adjacency via `bOnBlocker`).
- `AreAllCatsEliminated` - checks if all cat bits are set in the elimination mask

## Shape Definitions

Shapes are defined as templates (`TilePuzzleShapeDefinition`) containing a list of relative cell offsets from an origin point and a draggable flag. Predefined shapes are available in the `TilePuzzleShapes` namespace (`TilePuzzle_Types.h`) via `GetSingleShape`/`GetDominoShape`/`GetLShape`/`GetTShape`/`GetIShape`/`GetSShape`/`GetZShape`/`GetOShape` (or `GetShape(eType)`), each returning the cell offsets for that polyomino:

| Shape | Type enum | Cells (offsets) |
|-------|-----------|-----------------|
| Single | `TILEPUZZLE_SHAPE_SINGLE` | `{0,0}` |
| Domino | `TILEPUZZLE_SHAPE_DOMINO` | `{0,0},{1,0}` |
| L | `TILEPUZZLE_SHAPE_L` | `{0,0},{1,0},{2,0},{2,1}` |
| T | `TILEPUZZLE_SHAPE_T` | `{0,0},{1,0},{2,0},{1,1}` |
| I | `TILEPUZZLE_SHAPE_I` | `{0,0},{1,0},{2,0}` |
| S | `TILEPUZZLE_SHAPE_S` | `{1,0},{2,0},{0,1},{1,1}` |
| Z | `TILEPUZZLE_SHAPE_Z` | `{0,0},{1,0},{1,1},{2,1}` |
| O | `TILEPUZZLE_SHAPE_O` | `{0,0},{1,0},{0,1},{1,1}` |

`RotateShape90` rotates a definition 90° clockwise and re-normalizes the offsets.

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

## Pinball Minigame

`Components/Pinball_GameComponent.h` — A pinball-style minigame with a plunger launcher, physics ball, walls, curves, pegs, and a scoring target.

### Ball-Lost / Gate-Respawn Flow (Behaviour Graph)

The BALL_LOST decision flow lives in `Pinball_BallLostFlow.bgraph`
(boot-authored via `Zenith_EditorAutomation` graph steps in
`Project_RegisterEditorAutomationSteps`; attached to the Pinball scene's
manager entity with `AddStep_AttachGraph`; runtime docs in
`Zenith/Scripting/CLAUDE.md`). `Pinball_GameComponent` fires "BallLost" from
its `PINBALL_STATE_BALL_LOST` state — exactly where the old `HandleBallLost`
call sat — and `PinballHandleBallLost` (`Components/Pinball_GraphNodes.h`,
registered via `Pinball_RegisterGraphNodes()`) runs the retired body verbatim:

1. limited-ball gates decrement the ball counter,
2. objective met → `GateCleared()` (coins/save/celebration),
3. out of balls → `GateFailed()` (failure display, then retry reset),
4. otherwise → `RespawnBallFromGraph()` + READY.

The component keeps the systems as public graph-facing methods
(`GateCleared`/`GateFailed`/`RespawnBallFromGraph`, plus queries
`IsGateActive`/`GetGateMaxBalls`/`GetBallsRemaining`/`IsGateObjectiveMet`).

**Equivalence proof:** `Tests/Test_PinballCharacterization.cpp` —
`Pinball_RespawnFlow_Test` requests gate 5 (3-ball gate) via
`TilePuzzle::g_uPinballRequestedGate`, drives the REAL plunger mouse-drag
(Newton-inverting the camera's `ScreenSpaceToWorldSpace` to aim at the plunger),
drains 3 balls and asserts the 3→2→1→0 counter walk, the gate-failed display,
and the retry reset back to 3 balls + READY. Headless-runnable:

```
tilepuzzle.exe --automated-test Pinball_RespawnFlow_Test --headless --exit-after-frames 25000 --fixed-dt 0.01666 --skip-unit-tests
```

Gate on the Suite-summary line, not the process exit code — tilepuzzle has a
known layout-dependent pre-existing shutdown AV after "Shutdown complete"
(heap corruption freed at atexit by the module-scope `g_xResources`; tracked
as a task chip with the full diagnostic trail).

Caveat: `Pinball_GameComponent.h` is not self-contained — include
`TilePuzzle_GameComponent.h` first (it declares `TilePuzzle::Resources()`).

### Peg Layout System

Peg positions are **pre-generated during ZENITH_TOOLS boot** and saved to disk, then loaded at runtime. This ensures layouts are predetermined and logical rather than randomly scattered at runtime.

**Generation (tools only):**
- `Pinball_GameComponent::GenerateAndWriteLayouts()` is called from `Project_InitializeResources()`
- Generates 6 layouts, each with 8 pegs placed within the main playfield area
- Uses seeded `std::mt19937` (seed = `layoutIndex * 31337 + 42`) for deterministic output
- Enforces minimum 0.7 unit separation between pegs to prevent overlap
- Writes to `GAME_ASSETS_DIR "Pinball/PegLayouts.bin"`

**File format (`PegLayouts.bin`):**
```
uint32_t  layoutCount
[for each layout]
  uint32_t  pegCount
  [for each peg]
    float x, float y
```

**Runtime loading:**
- `LoadPegLayouts()` reads the binary file via `Zenith_DataStream::ReadFromFile()` during `CreatePlayfield()`
- `CreatePegs(layoutIndex)` spawns static sphere colliders from the selected layout
- `DestroyPegs()` removes all current peg entities

**Layout cycling:**
- On ball reset (`RespawnBall()`), the layout index increments (wrapping at `layoutCount`)
- Each attempt uses a different peg arrangement

## Engine Features Demonstrated

| Feature | Engine Class | Usage |
|---------|--------------|-------|
| **Game Components** | `Zenith_ComponentMetaRegistry` | Main game logic in `TilePuzzle_GameComponent` |
| **Scene Management** | `Zenith_SceneSystem` | Get active scene, create entities |
| **Entity Creation** | `Zenith_Entity` | Floor tiles, shapes, cats |
| **Prefab System** | `Zenith_Prefab` | Template-based entity instantiation |
| **Components** | `Zenith_TransformComponent`, `Zenith_ModelComponent`, `Zenith_UIComponent` | Position, rendering, UI |
| **Input System** | `Zenith_Input` | Keyboard and mouse/touch controls |
| **Multi-Scene** | `Zenith_SceneSystem` | `DontDestroyOnLoad()`, `LoadScene(..., SCENE_LOAD_ADDITIVE_WITHOUT_LOADING)`, `UnloadScene()` |
| **UI Buttons** | `Zenith_UIButton` | Clickable/tappable buttons with `SetOnClick()` callback |
| **Timed Destruction** | `Zenith_Entity::Destroy(delay)` | Cat elimination with 0.3s delay |
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
| T2.3 | Press Escape during gameplay | Returns to main menu |

### T3: Level Generation
| Step | Action | Expected Result |
|------|--------|-----------------|
| T3.1 | Play level 1 (Tutorial Early) | 5-6 x 5-6 grid, 1-2 colors, 1 cat/color, single-cell shapes only, level is solvable |
| T3.2 | Play level 6 (Tutorial Late) | 5-6 x 5-6 grid, 1-2 colors, multi-cell shapes (max size 2), level is solvable |
| T3.3 | Play level 81+ (Master) | 8x8 grid, 3 colors, 3 cats/color, all mechanics combined, level is solvable |
| T3.4 | Play levels spanning tier boundaries (e.g. 5/6, 10/11, 45/46) | Difficulty steps up at each tier boundary |
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
