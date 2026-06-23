# AI Navigation System

## Overview

The navigation system provides NavMesh-based pathfinding for AI agents. It generates walkable surfaces from scene geometry and provides A* pathfinding with path smoothing.

## Components

### Zenith_NavMesh

Stores the navigation mesh data structure:

- **Vertices**: World-space positions of polygon corners
- **Polygons**: Convex walkable areas with:
  - Vertex indices (CCW winding)
  - Neighbor indices (adjacent polygons)
  - Center point and normal
  - Area for cost calculations
- **Spatial Grid**: Acceleration structure for point queries

**Key Methods**:
- `FindNearestPolygon()`: Find polygon containing/nearest to a point
- `IsPointOnNavMesh()`: Check if a point is on walkable surface
- `Raycast()`: Test line-of-sight on navmesh
- `ProjectPoint()`: Project point onto navmesh surface

### Zenith_NavMeshGenerator

Generates navmesh from scene geometry using Recast-style pipeline:

1. **CollectGeometry**: Queries all static ColliderComponents for triangles
2. **Voxelization**: Converts triangles to 3D heightfield grid
3. **Span Filtering**: Removes non-walkable cells (steep slopes, low clearance)
4. **Region Building**: Flood-fills connected walkable areas
5. **Contour Tracing**: Extracts boundary polygons from regions
6. **Polygon Building**: Triangulates and builds convex polygon mesh
7. **Adjacency**: Computes neighbor relationships

**Configuration Parameters**:
| Parameter | Default | Description |
|-----------|---------|-------------|
| `m_fAgentRadius` | 0.4f | Agent collision radius |
| `m_fAgentHeight` | 1.8f | Agent height for clearance |
| `m_fMaxSlope` | 45.0f | Maximum walkable slope (degrees) |
| `m_fMaxStepHeight` | 0.3f | Maximum climbable step height |
| `m_fCellSize` | 0.3f | Voxel grid cell size |

### Zenith_Pathfinding

A* pathfinding on polygon graph:

- **Node**: Each polygon is a graph node
- **Edge Cost**: Euclidean distance between polygon centers
- **Heuristic**: Euclidean distance to goal polygon
- **Path Smoothing**: String-pulling via line-of-sight checks

**Path Results**:
- `SUCCESS`: Complete path found
- `PARTIAL`: Path to nearest reachable point
- `FAILED`: No path possible

### Zenith_NavMeshAgent

Handles agent movement along paths:

- **SetDestination()**: Requests path to position
- **Update()**: Advances along path each frame
- **CalculateVelocity()**: Computes desired velocity toward next waypoint
- Automatic path recalculation when destination changes
- **Turn toward travel**: while moving, `Update()` smoothly rotates the body to face its
  travel direction (`m_fTurnSpeed` deg/s) and writes a **pure-yaw** quaternion to the
  transform.

> **Heading derivation (don't regress this):** the current heading is read by rotating
> the +Z basis (`atan2((quat*+Z).x, (quat*+Z).z)`), NOT via `glm::eulerAngles(quat).y`.
> The euler yaw collapses for facings >~90° off +Z (a 180° facing decodes to yaw 0),
> which made −Z-bound agents turn the wrong way and re-encode a corrupted pitch=π/roll=π
> quaternion (fixed 2026-06; regression test `NavAgentFacingNegativeZTurnsTowardTravel`).
> The same `glm::eulerAngles().y` trap applies to any yaw-only heading logic.

**Configuration**:
| Setting | Default | Description |
|---------|---------|-------------|
| `m_fMoveSpeed` | 5.0f | Movement speed (m/s) |
| `m_fTurnSpeed` | 360.0f | Rotation speed (deg/s) |
| `m_fStoppingDistance` | 0.2f | Distance to stop from goal |
| `m_fAcceleration` | 20.0f | Acceleration rate |

## Usage Patterns

### Generating NavMesh

```cpp
NavMeshGenerationConfig xConfig;
xConfig.m_fAgentRadius = 0.4f;  // Match your agent size
xConfig.m_fAgentHeight = 1.8f;
xConfig.m_fMaxSlope = 45.0f;

// Engine-side collector (names Collider/Transform): scene geometry -> navmesh.
Zenith_NavMesh* pxNavMesh = Zenith_AINavGeometry::GenerateFromScene(xScene, xConfig);
// Pure-leaf path (raw geometry, no scene): Zenith_NavMeshGenerator::GenerateFromGeometry(verts, indices, xConfig).
```

### Pathfinding Query

```cpp
Zenith_PathResult xResult = Zenith_Pathfinding::FindPath(xNavMesh, xStart, xEnd);
if (xResult.m_eStatus == Zenith_PathResult::SUCCESS)
{
    for (auto& xWaypoint : xResult.m_axWaypoints)
    {
        // Process waypoints
    }
}
```

### Agent Movement

```cpp
Zenith_NavMeshAgent& xAgent = xEntity.GetComponent<Zenith_AIAgentComponent>().GetNavAgent();
xAgent.SetDestination(xTargetPos);

// In update loop
if (xAgent.HasReachedDestination())
{
    // Handle arrival
}
```

## Spatial Grid

The navmesh uses a 2D spatial grid for acceleration:

- Divides world XZ plane into cells
- Each cell stores polygon indices that overlap it
- Point queries only check relevant cell's polygons
- Cell size configurable (default 10x10 units)

## Path Smoothing Algorithm

String-pulling removes unnecessary waypoints:

1. Start with raw A* path (polygon centers)
2. For each waypoint, check LOS to waypoints ahead
3. Skip intermediate waypoints if LOS exists
4. Results in smoother, shorter paths

## Debug Visualization

Enable via `Zenith_AIDebugVariables`:

- `s_bDrawNavMeshEdges`: Polygon wireframe
- `s_bDrawNavMeshPolygons`: Filled surfaces
- `s_bDrawAgentPaths`: Current paths as lines
- `s_bDrawPathWaypoints`: Waypoint markers

## Common Issues

**Holes in NavMesh**:
- Ensure ColliderComponents are marked static
- Check geometry isn't too steep (increase m_fMaxSlope)
- Verify agent radius isn't too large

**Path Not Found**:
- Verify start and end points are on navmesh
- Check for disconnected regions
- Ensure navmesh was generated successfully

**Agent Getting Stuck**:
- Increase stopping distance
- Check for dynamic obstacles blocking path
- Verify navmesh covers the area

**Agent Faces / Turns the Wrong Way (esp. when travelling toward −Z)**:
- The turn-to-travel heading must come from `quat * +Z`, never `glm::eulerAngles(quat).y`
  (which collapses past ±90° off +Z). A symptom is an agent that spins or faces backward
  only when its path heads into the −Z hemisphere. Writeback must be a pure-yaw
  `angleAxis(yaw, +Y)` — re-encoding a full euler triple bakes in spurious pitch/roll.

**NavMesh Polygons Not Visible / Rendering Below Geometry**:
- Check polygon normals are pointing UP (Y should be +1.0, not -1.0)
- Debug with: `Zenith_Log("normal=(%.2f,%.2f,%.2f)", xNormal.x, xNormal.y, xNormal.z)`
- If normals point down, the winding order in `BuildPolygonMesh` is wrong
- The visual offset multiplies by normal, so inverted normals push polygons DOWN

**Polygon Winding Order (Critical)**:
- Zenith uses counter-clockwise (CCW) winding when viewed from above (Y-up)
- For a quad with vertices: V0=bottom-left, V1=bottom-right, V2=top-right, V3=top-left
- **WRONG**: V0→V1→V2→V3 is clockwise → produces normal (0, -1, 0)
- **CORRECT**: V0→V3→V2→V1 is counter-clockwise → produces normal (0, +1, 0)
- This affects: debug visualization, spatial queries, pathfinding polygon tests

**Only Seeing Obstacle/Wall Polygons (No Floor)**:
- In `BuildPolygonMesh`, ensure ALL walkable spans are processed, not just the topmost
- The generator iterates through voxel columns; each column may have multiple spans
- Floor spans are at the bottom, obstacle tops are higher spans
- Check the height categorization debug log: "floor", "mid", "high" polygon counts

## Performance Notes

- NavMesh generation is expensive, do offline or at load
- Pathfinding is O(n log n) where n = polygon count
- Spatial grid makes point queries O(1) average
- Path smoothing is O(w^2) where w = waypoint count
