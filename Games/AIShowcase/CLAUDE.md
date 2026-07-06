# AIShowcase Game

## Overview

A tactical demonstration game showcasing all Zenith AI system features:
- **NavMesh navigation** around obstacles and terrain
- **Behavior tree decision-making** (patrol, investigate, engage, retreat)
- **Perception systems** (sight cones, hearing sounds, damage awareness)
- **Squad coordination** (formations, flanking, suppression)
- **Debug visualization toggles** to observe AI internals

## Engine Features Demonstrated

| Feature | Engine Class | Usage |
|---------|--------------|-------|
| **NavMesh Navigation** | `Zenith_NavMesh`, `Zenith_NavMeshAgent` | Pathfinding around obstacles |
| **Behaviour Graphs** | `Zenith_GraphComponent` (`.bgraph`) | Menu/state flow + the per-enemy chase/patrol brain (see below) |
| **Perception System** | `Zenith_PerceptionSystem` | Sight cones, hearing, damage awareness |
| **Squad Tactics** | `Zenith_Squad`, `Zenith_Formation` | Formations, roles, coordinated flanking |
| **Tactical Points** | `Zenith_TacticalPointSystem` | Cover and flanking position evaluation |
| **Debug Visualization** | `Zenith_AIDebugVariables` (namespace, not a class — static toggle variables) | Toggle visualization of AI internals |
| **Multi-Scene** | `Zenith_SceneSystem` | `LoadScene(..., SCENE_LOAD_ADDITIVE_WITHOUT_LOADING)`, `SetActiveScene()`, `UnloadScene()`, `SetScenePaused()` |
| **Menu Buttons** | `Zenith_UIButton` | Clickable/tappable menu buttons with `SetOnClick()` callback |

## Project Structure

```
Games/AIShowcase/
├── CLAUDE.md                    # This file
├── AIShowcase.cpp               # Entry points, resource initialization
├── Components/
│   └── AIShowcase_GameComponent.h # Main game coordinator component
└── Assets/
    └── Scenes/
        └── AIShowcase.zscen     # Generated scene file
```

## Controls

| Key | Action |
|-----|--------|
| WASD | Move player |
| Space | Attack/Make sound (emits hearing stimulus) |
| 1-5 | Change squad formation (Line, Wedge, Column, Circle, Skirmish) |
| Click / Touch | Select menu button (single Play button, kept focused for activation) |
| Escape | Return to menu / Pause |
| P | Pause/Resume |
| R | Reset demo |

## AI Features Demonstrated

### NavMesh Navigation
- Arena with walls, ramps, and obstacles
- Agents pathfind around obstacles to reach targets
- Debug view shows NavMesh polygon edges

### Enemy Brain (Behaviour Graph)
Each enemy carries `AIShowcase_EnemyBrain.bgraph` (attached at runtime in
`CreateEnemy`). A reactive `Selector` decides each tick:
- **Chase** - if the enemy perceives the player, share the sighting with its
  squad and set the nav destination to the player's last-known position.
- **Patrol** - otherwise move between the four waypoints.

The decision is fired synchronously from `UpdateEnemyAI` Phase 1 via the
`"EnemyBrainTick"` custom event (patrol timer on the payload) - exactly where
the old inline decision sat - so the batch pathfinding and nav movement stay in
C++ (systems). See the Behaviour Graphs section below.

### Perception System

#### Sight
- 90-degree FOV with line-of-sight checks
- Awareness increases when player is visible
- Memory decay when target is out of sight

#### Hearing
- Player footsteps and attacks emit sounds
- Agents investigate sound sources
- Sound attenuates with distance

#### Damage Awareness
- Instant full awareness of attacker
- Information shared with squad

### Squad Tactics

#### Formations
1. **Line** - Spread horizontally
2. **Wedge** - V-shape with leader at front
3. **Column** - Single file
4. **Circle** - Defensive perimeter
5. **Skirmish** - Combat spread

#### Roles

The engine `SquadRole` enum (`Zenith/AI/Squad/Zenith_Formation.h`) defines six roles:
- **Leader** - Commands squad, others follow
- **Assault** - Front-line combat
- **Support** - Suppressing fire
- **Flanker** - Attacks from sides
- **Overwatch** - Long-range cover
- **Medic** - Support/healing role

AIShowcase only demonstrates three of them — `CreateSquad()` assigns **Leader**, **Assault**, and **Flanker** (see `AIShowcase_GameComponent.h`).

#### Coordination
- Shared target knowledge between squad members
- Coordinated flanking maneuvers
- Cover-to-cover movement

### Debug Visualization

Control via **Debug Variables panel** (available in Tools builds) under `AI` category.
Access via the AIShowcase properties panel checkbox or Zenith_DebugVariables system.

#### NavMesh Visualization
| Toggle | Color | Description |
|--------|-------|-------------|
| NavMesh Edges | Dark Green | Polygon wireframe edges |
| NavMesh Polygons | Green spheres | Polygon center markers |
| NavMesh Boundary | Red | Boundary edges (no neighbors) |
| NavMesh Neighbors | Blue | Lines connecting adjacent polygons |

#### Pathfinding Visualization
| Toggle | Color | Description |
|--------|-------|-------------|
| Agent Paths | Yellow | Current navigation path lines |
| Path Waypoints | Orange/Green | Waypoint markers (green = destination) |

#### Perception Visualization
| Toggle | Color | Description |
|--------|-------|-------------|
| Sight Cones | Yellow | Agent FOV cone outline |
| Hearing Radius | Blue | Circle showing hearing range |
| Detection Lines | Green→Red | Lines to targets (color = awareness level) |
| Memory Positions | Orange | Fading markers for lost targets |

#### Squad Visualization
| Toggle | Color | Description |
|--------|-------|-------------|
| Formation Positions | Role colors | Target formation slot spheres |
| Squad Links | Role colors | Lines from leader to members |
| Shared Targets | Red X | Known enemy positions |

#### Tactical Visualization
| Toggle | Color | Description |
|--------|-------|-------------|
| Cover Points | Green | Full/half cover positions |
| Flank Positions | Orange | Flanking tactical points |
| Tactical Scores | Yellow | Height indicators for point scores |

## Behaviour Graphs (W3 conversion)

Input/decision logic lives in two boot-authored graphs (built via
`Zenith_GraphBuilder` / `AddStep_GraphBuild` in `AIShowcase.cpp`); the
`AIShowcase_GameComponent` keeps the SYSTEMS (arena build, the perception/squad/
tactical/batch-pathfinding/nav-movement driver, UI, debug draw, WASD movement).

**`AIShowcase_GameFlow.bgraph`** (attached to the GameManager in BOTH scenes;
`gameState` = MENU by default, `StartGame` writes PLAYING). The game state LIVES
on this graph's blackboard - `GetGameState`/`SetGameState` read/write it.
- `OnUIButtonClicked("MenuPlay")` → `LoadSceneByIndex(1)` (engine UIButton
  trampoline - no C++ SetOnClick); `OnUpdate` → `AIShowcaseFocusPlayButton`.
- `OnKeyPressed(P)` → `SwitchOnInt(gameState)` → PLAYING↔PAUSED (`SetBlackboardInt`).
- `OnKeyPressed(Esc)` → `SwitchOnInt(gameState)` → `AIShowcaseReturnToMenu` +
  `SetBlackboardInt(gameState, MENU)` (setting MENU is required so the systems
  component reads MENU that same frame and skips `UpdateAISystems` - ReturnToMenu
  shut the AI systems down).
- `OnKeyPressed(Space/1-5/R)` → `CompareBlackboardInt(gameState==PLAYING)` →
  `Branch` → `AIShowcaseEmitPlayerSound` / `AIShowcaseSetFormation(i)` /
  `AIShowcaseResetDemo` (the PLAYING gate keeps R from rebuilding an arena in the menu).
- `OnUpdate` → `StateMachine(gameState, prefix "AIS")` LAST in node order → fires
  `AISEnter_Paused`/`AISExit_Paused` → `AIShowcaseSetArenaPaused(true/false)`
  (arena pause decoupled from the key chains).

**`AIShowcase_EnemyBrain.bgraph`** (per enemy, attached at runtime in
`CreateEnemy`; blackboard seeded with `playerTarget` = packed player id,
`enemyIndex` = the enemy's slot). Driven by `"EnemyBrainTick"` (patrol timer on
the payload) fired from `UpdateEnemyAI` Phase 1:
- `OnCustomEvent("EnemyBrainTick")` → `Selector(2, reactive, abortPreempted=false)`:
  - branch 0 (chase): `AIShowcaseSensePlayer` → `AIShowcaseShareTargetWithSquad`
    → `SetNavDestination(chaseDest)`.
  - branch 1 (patrol): `AIShowcasePatrol`.
  No RUNNING leaves ⇒ the Selector fully re-evaluates each tick; `abortPreempted=false`
  per the shared-nav-agent guidance.

### Game node library (`Components/AIShowcase_GraphNodes.h`, 9 nodes)

| Node | Role |
|---|---|
| `AIShowcaseFocusPlayButton` | Keeps the single menu button focused |
| `AIShowcaseReturnToMenu` | Teardown arena + load menu (the old ReturnToMenu) |
| `AIShowcaseResetDemo` | Rebuild + re-initialise the arena |
| `AIShowcaseSetArenaPaused` | Pause/unpause the arena scene (StateMachine seam) |
| `AIShowcaseEmitPlayerSound` | Emit the player's hearing stimulus (Space) |
| `AIShowcaseSetFormation` | Select a squad formation (1-5) |
| `AIShowcaseSensePlayer` | Perceive the player → write `chaseDest`; FAILURE if not seen |
| `AIShowcaseShareTargetWithSquad` | Share the sighting with the enemy's squad |
| `AIShowcasePatrol` | Idle/arrived → pick the next waypoint (enemyIndex + timer) |

GameFlow nodes resolve the GameManager's `AIShowcase_GameComponent` (self);
EnemyBrain nodes work off the enemy entity + the AI systems directly.

### Two pre-existing bugs fixed by this conversion
- **Esc-during-play crash**: `OnUpdate` PLAYING ran `HandlePlayerInput` (Esc →
  `ReturnToMenu` shuts the AI systems down) then unconditionally
  `UpdateAISystems` → `SquadManager::Update` asserted. The conversion skips
  systems on a transition frame (the component reads the new state first).
- **Perception dead on initial start**: `StartGame` (in `OnAwake`) registered the
  player target, then `OnStart`'s `Initialise()` cleared the whole registry and
  only the enemy *agents* were re-registered (by the engine `AIAgentComponent`) -
  so enemies patrolled forever. `StartGame` is now deferred to `OnStart` (after
  `Initialise`), matching `ResetDemo`'s correct init-then-register order.

### Equivalence proof
`Tests/Test_AIShowcaseCharacterization.cpp` (WINDOWED - `requiresGraphics=true`;
run one process per test with `--skip-unit-tests` because `--all-automated-tests`
trips the game's global AI-singleton teardown across tests). Written GREEN
against the (bug-fixed) C++ FIRST, then passing unchanged against the graphs:
`AIShowcase_MenuPlay_Test`, `_PauseResume_Test`, `_EscapeMenu_Test`,
`_PlayerMove_Test`, `_Formation_Test`, `_EnemyChase_Test`, `_EnemyPatrol_Test`.

```
aishowcase.exe --automated-test <name> --skip-unit-tests --fixed-dt 0.01666
```

## Multi-Scene Architecture

### Entity Layout
- **Persistent scene**: `GameManager` entity with Camera + UIComponent + AIShowcaseGame component (AIShowcase_GameComponent). Created at boot and persists architecturally as the Arena scene loads additively; survives scene transitions.
- **Arena scene** (`m_xArenaScene`, named "Arena"): Contains all level entities (floor, walls, obstacles, player, enemies). Loaded additively on play (`SCENE_LOAD_ADDITIVE_WITHOUT_LOADING`), unloaded on return to menu.

### Game State Machine
```
MAIN_MENU --> PLAYING --> PAUSED --> PLAYING
                             |
                             v
                         MAIN_MENU
```

### Scene Transition Pattern

Uses `LoadScene("Arena", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING)` + `SetActiveScene()` to start gameplay, `SetScenePaused(scene, true/false)` for pause/resume, and `LoadSceneByIndex(0, SCENE_LOAD_SINGLE)` to return to menu (`UnloadScene(m_xArenaScene)` is called during arena cleanup, not as the return-to-menu mechanism).

### Editor View / Scene Hierarchy
- **GameManager** - Persistent entity (Camera + UIComponent + AIShowcaseGame component)
- *(Arena scene "Arena", created at runtime)*
  - **Floor** - Arena ground plane with collider
  - **Wall_X** - Arena boundary walls
  - **Obstacle_X** - Cover obstacles
  - **Player** - Player entity (blue cylinder)
  - **Enemy_X** - AI agent entities (red/gold/orange cylinders)

## Arena Layout

```
┌────────────────────────────────────────┐
│                  Wall                   │
│  ┌───┐                          ┌───┐  │
│  │Obs│   ┌─────┐    ┌─────┐    │Obs│  │
│  └───┘   │ Obs │    │ Obs │    └───┘  │
│                                         │
│    ■    ┌──┐        ┌──┐     ■        │
│         │Ob│        │Ob│               │
│         └──┘        └──┘               │
│                                         │
│          ┌──────────────┐              │
│          │   Obstacle   │              │
│          └──────────────┘              │
│  ┌───┐                          ┌───┐  │
│  │Obs│      [Player Start]     │Obs│  │
│  └───┘                          └───┘  │
└────────────────────────────────────────┘
```

## Resources

Members of the `AIShowcaseResources` struct (`AIShowcase_GameComponent.h`), accessed via `AIShowcase::Resources()`.

### Geometry
- `m_pxCubeGeometry` - Unit cube for obstacles/walls
- `m_pxSphereGeometry` - Sphere for effects
- `m_pxCylinderGeometry` - Cylinder for agents

### Materials
These are `MaterialHandle` values (`m_x` prefix), not pointers.
- `m_xFloorMaterial` - Dark gray floor
- `m_xWallMaterial` - Brown walls
- `m_xObstacleMaterial` - Gray obstacles
- `m_xPlayerMaterial` - Blue player
- `m_xEnemyMaterial` - Red enemies
- `m_xLeaderMaterial` - Gold squad leaders
- `m_xFlankerMaterial` - Orange flankers
- `m_xCoverPointMaterial` - Green cover-point markers
- `m_xPatrolPointMaterial` - Blue patrol-point markers

## Building

1. Regenerate solution (`Build/` is the repo root directory — from `Games/AIShowcase/` use `cd ../../Build`):
   ```batch
   cd Build
   Sharpmake_Build.bat
   ```

2. Build the project:
   ```batch
   msbuild aishowcase_win64.sln /p:Configuration=vs2022_Debug_Win64_True /p:Platform=x64
   ```

3. Run:
   ```batch
   cd Games\AIShowcase\Build\output\win64\vs2022_debug_win64_true
   aishowcase.exe
   ```

## Extending

### Adding New Behaviors
1. Add a game node to `Components/AIShowcase_GraphNodes.h` (or reuse an engine
   node) and register it in `AIShowcase_RegisterGraphNodes()`.
2. Wire it into `BuildGraph_AIShowcaseEnemyBrain` (or `_GameFlow`) in
   `AIShowcase.cpp` via `Zenith_GraphBuilder`.
3. Per-instance state travels on the graph blackboard (seeded in `CreateEnemy`).

### Adding Enemies
1. Call `CreateEnemy()` with position and material
2. Add to squad via `pxSquad->AddMember()`
3. Register with perception system

### Adding Obstacles
1. Add to `aObstacles` array in `CreateObstacles()`
2. Regenerate NavMesh via `GenerateNavMesh()`
3. Add tactical points around new obstacles

## Test Plan

### T1: Menu Navigation
| Step | Action | Expected Result |
|------|--------|-----------------|
| T1.1 | Launch aishowcase.exe | Main menu displayed with Play button |
| T1.2 | Click Play button | Arena loads, menu hidden, HUD visible |
| T1.3 | Press Escape (while playing) | Returns to main menu, arena unloaded |

### T2: Scene Transitions
| Step | Action | Expected Result |
|------|--------|-----------------|
| T2.1 | Click Play from menu | Arena scene "Arena" created, entities spawned |
| T2.2 | Press Escape to return to menu | Arena scene unloaded, menu reappears |
| T2.3 | Click Play again | Fresh arena scene created, no stale AI state |
| T2.4 | Press R during gameplay | Arena destroyed and recreated (reset) |

### T3: Pause / Resume
| Step | Action | Expected Result |
|------|--------|-----------------|
| T3.1 | Press P during gameplay | Game pauses, "PAUSED" overlay shown |
| T3.2 | Press P again | Game resumes, overlay hidden |
| T3.3 | Press Escape while paused | Returns to main menu |
| T3.4 | Verify scene paused | AI agents, player movement frozen while paused |

### T4: Game Restart
| Step | Action | Expected Result |
|------|--------|-----------------|
| T4.1 | Press R during gameplay | Arena resets, player at start position |
| T4.2 | Verify AI state after reset | Squads re-initialized, enemies respawned |
| T4.3 | Click Play after Escape | Fresh arena, no leftover NavMesh or squads |

## See Also

- [Zenith/AI/CLAUDE.md](../../Zenith/AI/CLAUDE.md) - AI system overview
- [Zenith/AI/Navigation/CLAUDE.md](../../Zenith/AI/Navigation/CLAUDE.md) - NavMesh details
- [Zenith/AI/Perception/CLAUDE.md](../../Zenith/AI/Perception/CLAUDE.md) - Perception configuration
- [Zenith/AI/Squad/CLAUDE.md](../../Zenith/AI/Squad/CLAUDE.md) - Squad tactics
