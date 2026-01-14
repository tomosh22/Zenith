# Squad Tactics System

## Overview

The squad system provides coordinated group AI behavior including formations, role assignment, shared knowledge, and tactical decision-making.

## Components

### Zenith_Squad

Manages a group of AI agents working together:
- Member tracking with roles
- Formation positioning
- Shared target knowledge
- Coordinated orders

### Zenith_Formation

Defines spatial layouts for squad members:
- Slot positions relative to leader
- Role preferences per slot
- World-space position calculation

### Zenith_TacticalPointSystem

Manages tactical positions in the world:
- Cover points (full/half)
- Flank positions
- Overwatch positions
- Dynamic point evaluation

## Squad Roles

| Role | Description | Typical Behavior |
|------|-------------|------------------|
| LEADER | Commands squad | Front position, makes decisions |
| ASSAULT | Front-line combat | Close engagement, aggressive |
| SUPPORT | Fire support | Medium range, suppression |
| FLANKER | Flanking maneuvers | Circles around targets |
| OVERWATCH | Distant cover | Long range, elevated positions |
| MEDIC | Support role | Stays back, assists wounded |

## Formations

### Built-in Formations

**Line**: Members spread horizontally
```
    [2]  [0/L]  [1]  [3]  [4]
```

**Wedge**: V-shape with leader at front
```
        [0/L]
      [1]  [2]
    [3]      [4]
```

**Column**: Single file
```
[0/L]
[1]
[2]
[3]
```

**Circle**: Defensive perimeter
```
     [1]
  [5]   [2]
[4] [0/L] [3]
```

**Skirmish**: Spread for combat
```
  [1]     [2]
     [0/L]
  [3]     [4]
```

### Formation API

```cpp
// Set squad formation
pxSquad->SetFormation(Zenith_Formation::GetWedge());

// Get formation position for member
Zenith_Maths::Vector3 xPos = pxSquad->GetFormationPositionFor(xMemberID);

// Custom formation
Zenith_Formation xCustom("Custom");
xCustom.AddSlot(Vector3(0,0,0), SquadRole::LEADER, 10.0f);
xCustom.AddSlot(Vector3(2,0,-1), SquadRole::ASSAULT, 5.0f);
xCustom.SetSpacing(3.0f);
```

## Squad Orders

### Order Types

| Order | Description |
|-------|-------------|
| MOVE_TO | Move squad to position |
| ATTACK | Attack a target |
| DEFEND | Defend a position |
| FLANK | Flank a target |
| SUPPRESS | Suppress target area |
| REGROUP | Regroup at leader |
| RETREAT | Fall back to position |
| HOLD_POSITION | Stop and hold |

### Issuing Orders

```cpp
pxSquad->OrderMoveTo(xPosition);
pxSquad->OrderAttack(xTargetID);
pxSquad->OrderFlank(xTargetID);
pxSquad->OrderRegroup();
pxSquad->OrderRetreat(xFallbackPos);
```

## Shared Knowledge

Squad members share information about targets:

```cpp
// Share target sighting
pxSquad->ShareTargetInfo(xTargetID, xPosition, xReportingMember);

// Check if squad knows about target
if (pxSquad->IsTargetKnown(xTargetID))
{
    const auto* pxTarget = pxSquad->GetSharedTarget(xTargetID);
    Vector3 xLastKnown = pxTarget->m_xLastKnownPosition;
}

// Mark target as engaged
pxSquad->SetTargetEngaged(xTargetID, xEngagingMember);

// Get priority target (unengaged, most recent)
Zenith_EntityID xPriority = pxSquad->GetPriorityTarget();
```

## Tactical Point System

### Point Types

| Type | Description | Use Case |
|------|-------------|----------|
| COVER_FULL | Complete concealment | Avoid fire |
| COVER_HALF | Partial cover | Shoot while protected |
| FLANK_POSITION | Side attack position | Flanking maneuvers |
| OVERWATCH | Elevated view | Long-range cover |
| PATROL_WAYPOINT | Route marker | Patrol paths |
| AMBUSH | Hidden attack position | Surprise attacks |
| RETREAT | Safe fallback | Emergency retreat |

### Registering Points

```cpp
// Register cover point
uint32_t uID = Zenith_TacticalPointSystem::RegisterPoint(
    xPosition,
    TacticalPointType::COVER_HALF,
    xFacingDirection,
    xOwnerEntity  // Optional - for dynamic points
);

// Unregister
Zenith_TacticalPointSystem::UnregisterPoint(uID);
```

### Querying Points

```cpp
// Find best cover from threat
Vector3 xCover = Zenith_TacticalPointSystem::FindBestCoverPosition(
    xAgentID,
    xThreatPosition,
    20.0f  // Max distance
);

// Find flank position
Vector3 xFlank = Zenith_TacticalPointSystem::FindBestFlankPosition(
    xAgentID,
    xTargetPosition,
    xTargetFacing,
    5.0f,   // Min distance
    15.0f   // Max distance
);

// Find overwatch position
Vector3 xOverwatch = Zenith_TacticalPointSystem::FindBestOverwatchPosition(
    xAgentID,
    xAreaToWatch,
    10.0f,  // Min distance
    30.0f   // Max distance
);
```

### Point Occupation

```cpp
// Reserve point for future use
Zenith_TacticalPointSystem::ReservePoint(uPointID, xAgentID);

// Occupy point (agent arrived)
Zenith_TacticalPointSystem::OccupyPoint(uPointID, xAgentID);

// Release when leaving
Zenith_TacticalPointSystem::ReleasePoint(uPointID, xAgentID);
```

### Point Scoring

Points are scored based on query context:
- **Distance**: Closer to search center = higher
- **Cover**: Better protection from threat = higher
- **Visibility**: Clear sight lines = higher (for overwatch)
- **Elevation**: Higher positions = bonus

## Squad Manager

Global management of all squads:

```cpp
// Create new squad
Zenith_Squad* pxSquad = Zenith_SquadManager::CreateSquad("Alpha");

// Add members
pxSquad->AddMember(xEntity1, SquadRole::LEADER);
pxSquad->AddMember(xEntity2, SquadRole::ASSAULT);
pxSquad->AddMember(xEntity3, SquadRole::FLANKER);

// Find squad by name
Zenith_Squad* pxFound = Zenith_SquadManager::GetSquadByName("Alpha");

// Find squad containing entity
Zenith_Squad* pxEntitySquad = Zenith_SquadManager::GetSquadForEntity(xEntityID);

// Destroy squad
Zenith_SquadManager::DestroySquad(pxSquad);
```

## Debug Visualization

Enable via `Zenith_AIDebugVariables`:

- `s_bDrawFormationPositions`: Target positions as spheres
- `s_bDrawSquadLinks`: Lines between members
- `s_bDrawRoleLabels`: Role names above agents
- `s_bDrawSharedTargets`: Known target markers
- `s_bDrawCoverPoints`: Cover positions
- `s_bDrawFlankPositions`: Flank positions

## Integration with Behavior Trees

### Accessing Squad in Actions

```cpp
BTNodeStatus Execute(Zenith_Entity& xAgent, Zenith_Blackboard& xBB, float fDt)
{
    Zenith_Squad* pxSquad = Zenith_SquadManager::GetSquadForEntity(xAgent.GetEntityID());
    if (pxSquad)
    {
        // Get formation position
        Vector3 xFormPos = pxSquad->GetFormationPositionFor(xAgent.GetEntityID());

        // Check for shared targets
        Zenith_EntityID xTarget = pxSquad->GetPriorityTarget();
    }
}
```

### Role-Based Behavior

```cpp
// Different behavior based on role
SquadRole eRole = pxSquad->GetMemberRole(xAgentID);
switch (eRole)
{
case SquadRole::FLANKER:
    xBB.SetBool("ShouldFlank", true);
    break;
case SquadRole::SUPPORT:
    xBB.SetBool("ShouldSuppress", true);
    break;
}
```

## Performance Notes

- Formation updates every 0.5s by default
- Shared knowledge timeout: 30s
- Point scoring uses simple heuristics
- Dynamic point generation is expensive
