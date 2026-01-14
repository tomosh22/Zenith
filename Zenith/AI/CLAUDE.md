# Zenith AI System

## Overview

The AI system provides comprehensive game AI capabilities including:
- **Navigation**: NavMesh-based pathfinding with A* and path smoothing
- **Behavior Trees**: Flexible decision-making framework
- **Perception**: Sight cones, hearing, damage awareness, and memory
- **Squad Tactics**: Formations, roles, tactical points, and coordinated behavior

## Architecture

```
AI/
├── Navigation/          # NavMesh, pathfinding, agent movement
│   ├── Zenith_NavMesh.h/cpp
│   ├── Zenith_NavMeshGenerator.h/cpp
│   ├── Zenith_Pathfinding.h/cpp
│   └── Zenith_NavMeshAgent.h/cpp
├── BehaviorTree/        # Decision-making framework
│   ├── Zenith_BehaviorTree.h/cpp
│   ├── Zenith_BTNode.h/cpp
│   ├── Zenith_BTComposites.h/cpp
│   ├── Zenith_BTDecorators.h/cpp
│   ├── Zenith_BTActions.h/cpp
│   ├── Zenith_BTConditions.h/cpp
│   └── Zenith_Blackboard.h/cpp
├── Perception/          # Sensory systems
│   └── Zenith_PerceptionSystem.h/cpp
├── Squad/               # Tactical coordination
│   ├── Zenith_Squad.h/cpp
│   ├── Zenith_Formation.h/cpp
│   └── Zenith_TacticalPoint.h/cpp
└── Components/          # ECS integration
    └── Zenith_AIAgentComponent.h/cpp
```

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

### Behavior Tree System

**Node Types**:
- `Zenith_BTNode`: Abstract base with Execute(), OnEnter(), OnExit(), OnAbort()
- `Zenith_BTComposite`: Multiple children (Sequence, Selector, Parallel)
- `Zenith_BTDecorator`: Single child modifier (Inverter, Repeater, Cooldown)
- `Zenith_BTLeaf`: Actions and conditions

**Execution Model**:
- Trees tick at configurable interval (default 10 Hz)
- Nodes return SUCCESS, FAILURE, or RUNNING
- RUNNING nodes resume on next tick
- Blackboard provides shared state storage

**Blackboard** supports types:
- float, int32, bool, Vector3, EntityID
- Key-value storage with serialization

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
// Add the component
auto& xAI = xEntity.AddComponent<Zenith_AIAgentComponent>(xEntity);

// Configure behavior tree
xAI.SetBehaviorTree(pxPatrolTree);

// Set up blackboard
xAI.GetBlackboard().SetFloat("PatrolRadius", 10.0f);
xAI.GetBlackboard().SetVector3("HomePosition", xHomePos);

// Register with perception system
Zenith_PerceptionSystem::RegisterAgent(xEntity.GetEntityID());
Zenith_PerceptionSystem::SetSightConfig(xEntity.GetEntityID(), xSightConfig);
```

### Generating NavMesh

```cpp
NavMeshGenerationConfig xConfig;
xConfig.m_fAgentRadius = 0.4f;
xConfig.m_fAgentHeight = 1.8f;
xConfig.m_fMaxSlope = 45.0f;

Zenith_NavMesh* pxNavMesh = Zenith_NavMeshGenerator::GenerateFromScene(xScene, xConfig);
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

## Update Order

Per frame:
1. Physics update (provides collision data)
2. PerceptionSystem::Update() - processes senses
3. AIAgentComponent::OnUpdate() - ticks behavior trees
4. NavMeshAgent::Update() - moves agents

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

## Common Issues

**Agent not moving**: Check NavMesh is assigned to agent, destination is on navmesh

**Not detecting targets**: Ensure targets registered with PerceptionSystem::RegisterTarget()

**Holes in NavMesh**: Check ColliderComponents are static, geometry not too steep

## See Also

- [Navigation/CLAUDE.md](Navigation/CLAUDE.md) - NavMesh details
- [BehaviorTree/CLAUDE.md](BehaviorTree/CLAUDE.md) - BT node reference
- [Perception/CLAUDE.md](Perception/CLAUDE.md) - Perception configuration
