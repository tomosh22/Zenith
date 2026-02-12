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
| **Behavior Trees** | `Zenith_BehaviorTree`, `Zenith_Blackboard` | Patrol, investigate, engage, retreat AI |
| **Perception System** | `Zenith_PerceptionSystem` | Sight cones, hearing, damage awareness |
| **Squad Tactics** | `Zenith_Squad`, `Zenith_Formation` | Formations, roles, coordinated flanking |
| **Tactical Points** | `Zenith_TacticalPointSystem` | Cover and flanking position evaluation |
| **Debug Visualization** | `Zenith_AIDebugVariables` | Toggle visualization of AI internals |
| **Multi-Scene** | `Zenith_SceneManager` | `DontDestroyOnLoad()`, `CreateEmptyScene()`, `UnloadScene()`, `SetScenePaused()` |
| **Menu Buttons** | `Zenith_UIButton` | Clickable/tappable menu buttons with `SetOnClick()` callback |

## Project Structure

```
Games/AIShowcase/
├── CLAUDE.md                    # This file
├── AIShowcase.cpp               # Entry points, resource initialization
├── Components/
│   └── AIShowcase_Behaviour.h   # Main game coordinator
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
| Click / Touch | Select menu button |
| W/S or Up/Down | Navigate menu |
| Enter | Activate focused button |
| Escape | Return to menu / Pause |
| P | Pause/Resume |
| R | Reset demo |

## AI Features Demonstrated

### NavMesh Navigation
- Arena with walls, ramps, and obstacles
- Agents pathfind around obstacles to reach targets
- Debug view shows NavMesh polygon edges

### Behavior Trees
- **Patrol** - Move between waypoints
- **Investigate** - Check sounds/last known positions
- **Engage** - Move toward and attack player
- **Retreat** - Fall back when health is low

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
- **Leader** - Commands squad, others follow
- **Assault** - Front-line combat
- **Flanker** - Attacks from sides
- **Support** - Suppressing fire
- **Overwatch** - Long-range cover

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

## Multi-Scene Architecture

### Entity Layout
- **Persistent scene** (DontDestroyOnLoad): `GameManager` entity with Camera + UIComponent + ScriptComponent (AIShowcase_Behaviour). Survives scene transitions.
- **Arena scene** (`m_xArenaScene`, named "Arena"): Contains all level entities (floor, walls, obstacles, player, enemies). Created on play, destroyed on return to menu.

### Game State Machine
```
MAIN_MENU --> PLAYING --> PAUSED --> PLAYING
                             |
                             v
                         MAIN_MENU
```

### Scene Transition Pattern

Uses `CreateEmptyScene("Arena")` + `SetActiveScene()` to start gameplay, `SetScenePaused(scene, true/false)` for pause/resume, and `UnloadScene()` to return to menu.

### Editor View / Scene Hierarchy
- **GameManager** - Persistent entity (Camera + UIComponent + ScriptComponent/AIShowcase_Behaviour) - DontDestroyOnLoad
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

### Geometry
- `g_pxCubeGeometry` - Unit cube for obstacles/walls
- `g_pxSphereGeometry` - Sphere for effects
- `g_pxCylinderGeometry` - Cylinder for agents

### Materials
- `g_pxFloorMaterial` - Dark gray floor
- `g_pxWallMaterial` - Brown walls
- `g_pxObstacleMaterial` - Gray obstacles
- `g_pxPlayerMaterial` - Blue player
- `g_pxEnemyMaterial` - Red enemies
- `g_pxLeaderMaterial` - Gold squad leaders
- `g_pxFlankerMaterial` - Orange flankers

## Building

1. Regenerate solution:
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
1. Create behavior tree in code (or load from .zbtree)
2. Assign to AIAgentComponent via `SetBehaviorTree()`
3. Use blackboard keys for state

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
| T1.4 | Use W/S or Up/Down arrows | Menu button focus navigates |
| T1.5 | Press Enter on focused button | Activates the focused button |

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
- [Zenith/AI/BehaviorTree/CLAUDE.md](../../Zenith/AI/BehaviorTree/CLAUDE.md) - Behavior tree reference
- [Zenith/AI/Perception/CLAUDE.md](../../Zenith/AI/Perception/CLAUDE.md) - Perception configuration
- [Zenith/AI/Squad/CLAUDE.md](../../Zenith/AI/Squad/CLAUDE.md) - Squad tactics
