# Zenith AI System

## Overview

The AI system provides comprehensive game AI capabilities including:
- **Navigation**: NavMesh-based pathfinding with A* and path smoothing
- **Perception**: Sight cones, hearing, damage awareness, and memory
- **Squad Tactics**: Formations, roles, tactical points, and coordinated behavior

> Decision-making is authored in **Behaviour Graphs** (`Zenith/Scripting/`,
> `Zenith_GraphComponent`), not here. The former `Zenith/AI/BehaviorTree` module
> was removed once every game had converted (doctrine: systems = C++, logic =
> graphs); `Zenith_AIAgentComponent` survives as a perception/nav host only.

## Architecture

```
AI/
├── Navigation/          # NavMesh, pathfinding, agent movement
│   ├── Zenith_NavMesh.h/cpp
│   ├── Zenith_NavMeshGenerator.h/cpp
│   ├── Zenith_Pathfinding.h/cpp
│   └── Zenith_NavMeshAgent.h/cpp
├── Perception/          # Sensory systems
│   └── Zenith_PerceptionSystem.h/cpp
├── Squad/               # Tactical coordination
│   ├── Zenith_Squad.h/cpp
│   ├── Zenith_Formation.h/cpp
│   └── Zenith_TacticalPoint.h/cpp
└── Zenith_AIWorldHooks.h/cpp   # leaf->engine DI seam (entity pos/rot, collider
                                #   body, nav agent, parallel-for, TOOLS debug-draw)
```

> **`Zenith/AI/` is the strict-leaf `ZenithAI` static library** (over
> ZenithBase + ZenithECS + ZenithPhysics). It names no concrete component, no Flux
> type, no `g_xEngine`, and no `TaskSystem` — it reaches the engine only through the
> `Zenith_AIWorldHooks` function-pointer seam (wired engine-side by
> `EntityComponent/Zenith_AIWorldHooksInstall.cpp`). The concrete
> **`Zenith_AIAgentComponent`** ECS component (which names Transform/Collider) now
> lives **engine-side** in `EntityComponent/Components/`, NOT here. Scene→navmesh
> collection lives engine-side too (`EntityComponent/Zenith_AINavGeometry`); the leaf
> generator only takes raw geometry. Proven a clean leaf by `SentinelAI` +
> `dumpbin FORBIDDEN_EXTERNALS=NONE`.

## Subsystem Overview

### Navigation System

**NavMesh** stores walkable polygons with:
- Vertex positions and polygon connectivity
- Spatial acceleration grid for fast point queries
- Serialization to `.znavmesh` files

**NavMeshGenerator** builds navmeshes from scene geometry:
1. Collects static ColliderComponent geometry
2. Voxelizes triangles into 3D heightfield
3. Filters by slope and step height
4. Builds connected regions via flood fill
5. Traces contours and builds polygon mesh
6. Computes adjacency for pathfinding

**Pathfinding** uses A* on polygon graph:
- Heuristic: Euclidean distance to goal
- Edge cost: Distance * polygon cost multiplier
- Path smoothing via line-of-sight checks

**NavMeshAgent** handles movement:
- Path following with waypoint advancement
- Smooth acceleration/deceleration
- Rotation towards movement direction

### Decision-making (Behaviour Graphs)

Agent decision-making is authored in Behaviour Graphs (`Zenith/Scripting/`,
hosted by `Zenith_GraphComponent`), not in this module. The graph runtime
provides the BT-equivalent constructs — a reactive `Selector`, `StateMachine`,
`Repeat`, `Cooldown`, RUNNING suspension/resume, and AI leaf nodes (`NavMoveTo`,
`SetNavDestination`, perception queries) in `Zenith_GraphNode_Registration_AI.cpp`.
See `Zenith/Scripting/CLAUDE.md`.

### Perception System

**Sight Perception**:
- Configurable FOV (primary + peripheral)
- Range-limited with distance falloff
- Line-of-sight raycasts (optional)
- Awareness gain/decay over time

**Hearing Perception**:
- Sound stimuli with position, loudness, radius
- Distance attenuation
- Loudness threshold for detection

**Damage Perception**:
- Immediate full awareness of attacker
- Tracks who damaged the agent

**Memory**:
- Last known position per target
- Awareness decays when target not visible
- Targets forgotten when awareness reaches zero

## Usage

### Adding AI to an Entity

```cpp
// Add the component (a perception/nav host; decisions live in a Behaviour Graph)
auto& xAI = xEntity.AddComponent<Zenith_AIAgentComponent>();

// Wire a nav agent (non-owning) so the graph's nav nodes can drive movement
xAI.SetNavMeshAgent(&xMyNavAgent);

// Register with perception system
Zenith_PerceptionSystem::RegisterAgent(xEntity.GetEntityID());
Zenith_PerceptionSystem::SetSightConfig(xEntity.GetEntityID(), xSightConfig);

// Author the decision graph on a Zenith_GraphComponent (see Zenith/Scripting/)
```

### Generating NavMesh

```cpp
NavMeshGenerationConfig xConfig;
xConfig.m_fAgentRadius = 0.4f;
xConfig.m_fAgentHeight = 1.8f;
xConfig.m_fMaxSlope = 45.0f;

// Engine-side collector (EntityComponent/) gathers scene collider geometry and
// calls the leaf generator. The pure leaf entry point is
// Zenith_NavMeshGenerator::GenerateFromGeometry(verts, indices, xConfig).
Zenith_NavMesh* pxNavMesh = Zenith_AINavGeometry::GenerateFromScene(xScene, xConfig);
pxNavMesh->SaveToFile("navmesh.znavmesh");
```

### Pathfinding

```cpp
Zenith_PathResult xResult = Zenith_Pathfinding::FindPath(*pxNavMesh, xStart, xEnd);
if (xResult.m_eStatus == Zenith_PathResult::Status::SUCCESS)
{
    // Use xResult.m_axWaypoints
}
```

### Querying Perception

```cpp
// Get all perceived targets
const auto* pxTargets = Zenith_PerceptionSystem::GetPerceivedTargets(xAgentID);

// Get highest-awareness hostile target
Zenith_EntityID xTarget = Zenith_PerceptionSystem::GetPrimaryTarget(xAgentID);

// Check awareness of specific entity
float fAwareness = Zenith_PerceptionSystem::GetAwarenessOf(xAgentID, xTargetID);
```

## Initialization Order

In `Zenith_Core.cpp`:
1. Physics system initializes first
2. Perception system: `Zenith_PerceptionSystem::Initialise()`
3. Scene loads, entities created
4. NavMesh generated or loaded
5. AI components initialize

## Update Order — who ticks the AI managers

**The AI manager systems are GAME-driven by default, not engine-driven.**
`Zenith_PerceptionSystem::Update()`, `Zenith_SquadManager::Update()`, and
`Zenith_TacticalPointSystem::Update()` are NOT called by `Zenith_Core::Zenith_MainLoop`
out of the box — each game ticks them from its own component, in a game-specific
order relative to its per-agent AI step (e.g. a game's coordinator component
ticks perception → squad → tactical-point BEFORE its enemy-AI loop, so agents act
on fresh perception). That ordering is intentional and is why the engine does not
tick them unconditionally — a newcomer must drive them (or opt in below) for
perception to run.

**Opt-in engine tick:** a game with no such ordering constraint can call
`Zenith_AI::SetEngineTickEnabled(true)` once at init; the main loop then calls
`Zenith_AI::Update(dt)` (perception → squad → tactical-point) each game-logic frame,
after the scene update. Do not also tick the managers from game code when enabled.

The typical per-frame sequence (whichever drives the managers):
1. Physics update (provides collision data)
2. AI managers: `PerceptionSystem::Update()` (+ squad / tactical-point) — game-driven, or the opt-in engine tick
3. Behaviour-graph decision tick (`Zenith_GraphComponent`, e.g. a "PriestTick" event) sets nav destinations
4. `AIAgentComponent::OnUpdate()` → `NavMeshAgent::Update()` - moves agents along the path

## Debug Visualization

Enable via Zenith_DebugVariables panel:
- **AI/NavMesh/Wireframe Edges**: NavMesh polygon edges
- **AI/Pathfinding/Agent Paths**: Current paths
- **AI/Perception/Sight Cones**: FOV visualization
- **AI/Perception/Detection Lines**: Lines to perceived targets

## Performance Considerations

- Behavior trees tick at 10 Hz by default, not every frame
- Perception updates stagger agents to distribute load
- NavMesh spatial grid accelerates point queries
- Path smoothing reduces waypoint count

## Facing / heading convention (cross-cutting)

AI code that needs an agent's forward/heading derives it by rotating the +Z basis with
the entity's transform quaternion — `forward = quat * (0,0,1)` (project to XZ;
`yaw = atan2(forward.x, forward.z)` if a scalar yaw is needed). **Never extract yaw via
`glm::eulerAngles(quat).y`**: that asin-based middle angle collapses for facings more
than ~90° off +Z (a 180°/−Z facing decodes to yaw 0), silently reversing the heading.
This bit BOTH the perception sight cone (agents blind to targets in front when facing −Z)
and nav turn-to-travel (wrong-way turns + corrupted pitch/roll) — fixed 2026-06 with
regression tests in each. Apply the same rule to any new yaw-only logic.

## Common Issues

**Agent not moving**: Check NavMesh is assigned to agent, destination is on navmesh

**Not detecting targets**: Ensure targets registered with PerceptionSystem::RegisterTarget().
If detection fails only at certain facings, suspect a `glm::eulerAngles().y` forward
derivation (see *Facing / heading convention* above).

**Holes in NavMesh**: Check ColliderComponents are static, geometry not too steep

**Agent faces the wrong way travelling toward −Z**: nav heading must use `quat*+Z`, not
`glm::eulerAngles().y` (see *Facing / heading convention* above).

## See Also

- [Navigation/CLAUDE.md](Navigation/CLAUDE.md) - NavMesh details
- [Perception/CLAUDE.md](Perception/CLAUDE.md) - Perception configuration
