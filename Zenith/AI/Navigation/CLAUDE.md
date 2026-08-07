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
- `GetRandomReachablePointInRadius()`: Sample a uniformly-random walkable point path-connected to a center within a horizontal radius (Unreal-style; disconnected islands excluded). **Deterministic**: a per-instance, fixed-seed xorshift64* stream (NOT `std::random_device` — it was, which made every process run differ), rewound by `Clear()`. The stream still ADVANCES per call, so repeated queries give different points. Main-thread (graph-tick) use only. Pinned by `NavMeshRandomPointSamplingIsDeterministic`.

**Dynamic obstacles** (carve transient obstacles like doors without regenerating the mesh):
- `SetPolygonBlocked()`: Toggle a polygon's BLOCKED flag (skipped by `FindPath`); `const`, mutates flag via internal `const_cast`
- `SetBlockedAtPoint()`: Block/unblock every polygon whose footprint contains a world point
- `StitchPortalAt()`: Add a mutual neighbor link between walkable polygons either side of a point (bridges wall-separated regions; used by DPDoor)

### Zenith_NavMeshGenerator

Generates navmesh from raw geometry using Recast-style pipeline. Geometry
collection is **engine-side**: `Zenith_AINavGeometry::GenerateFromScene` gathers
triangles from static ColliderComponents and calls the leaf entry point
`GenerateFromGeometry(verts, indices, xConfig)`, which begins at voxelization:

1. **Voxelization**: Converts triangles to 3D heightfield grid
2. **Span Filtering**: Removes non-walkable cells (steep slopes, low clearance)
3. **Build Compact Heightfield**: Packs walkable spans into a flat, cache-friendly array indexed by (x, z) for efficient flood-fill and contour operations
4. **Region Building**: Flood-fills connected walkable areas
5. **Contour Tracing**: Traces per-region boundary contours (currently populated but not consumed — polygon building works directly from spans)
6. **Polygon Building**: Builds convex quad polygon mesh from walkable spans (one quad per walkable span, CCW winding); does not use the traced contours
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
- **Edge Cost**: Euclidean distance between polygon centers, multiplied by the neighbor polygon's cost multiplier (`m_fCost`)
- **Heuristic**: Euclidean distance to the projected endpoint (the goal point projected onto the nearest polygon, not the polygon center)
- **Path Smoothing**: String-pulling via line-of-sight checks

**Path Results**:
- `SUCCESS`: Complete path found
- `PARTIAL`: Path to nearest reachable point
- `FAILED`: No path possible

**Batch API**: `FindPathsBatch(PathRequest*, uNumRequests)` computes many paths in
parallel via `Zenith_DataParallelTask`, writing each result into `PathRequest::m_xResult`;
blocks until all are done.

**Test harness**: `Zenith_NavMeshTestPathfinder.h` is a test-only shim (gated behind
`ZENITH_INPUT_SIMULATOR`) wrapping `Zenith_Pathfinding::FindPath` so test bots route on the
same navmesh the production agents use; not part of the runtime path.

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

## Baked navmesh persistence (ZM-D-147)

The bake → commit → load workflow, and the reason it exists: a baked `.znavmesh`
loads with **no GPU, no terrain component and no scene geometry**, so navigation
becomes CI-verifiable on a GPU-less runner. This is the engine's answer to
Unity's `NavMeshSurface` and Unreal's `ARecastNavMesh`.

### The three pieces

| Piece | Role |
|---|---|
| `Zenith_NavMeshBaker::BakeToFile` | TOOLS-time: geometry → generate → serialize → write → **read back + memcmp** → typed result |
| `Zenith_NavMeshComponent` (order 96) | RUNTIME: carries the asset ref, loads in `OnStart`, **owns** the mesh for its own lifetime |
| `Zenith_NavMeshStats::Compute` | Read-only summary (counts, bounds, areas, portals, isolated/blocked) — what the editor panel formats |

`BakeToFile`'s read-back is not paranoia: `Zenith_FileAccess::WriteFile` returns
`void`, so comparing the bytes on disk to the bytes we meant to write is the only
truthful success signal available.

### Two adoption recipes

```cpp
// 1. AUTHORED scenes -- the component is serialized INTO the scene.
//    In editor automation, before AddStep_SaveScene:
xAuto.AddStep_CreateEntity("MyLevelNavMesh");
xAuto.AddStep_AddComponent("NavMesh");        // needs the editor-registry mirror!
xAuto.AddStep_Custom(&ConfigureMyNavMesh);    // captureless: SetAssetRef(...)

// 2. RUNTIME scenes -- add and point it at the asset in code.
Zenith_NavMeshComponent& xNav = xEntity.AddComponent<Zenith_NavMeshComponent>();
xNav.SetAssetRef("game:Navmesh/MyLevel.znavmesh");   // loads immediately

// Discovery is an ordinary ECS query -- there is no registry to consult.
g_xEngine.Scenes().QueryActiveScene<Zenith_NavMeshComponent>().ForEach(
    [](Zenith_EntityID, Zenith_NavMeshComponent& xNav) { /* xNav.GetNavMesh() */ });
```

**Ownership is the component's, deliberately.** There is no path-keyed cache and
no static state anywhere in this feature: a navmesh's lifetime is its level's, so
scene switching frees and re-loads automatically and there is nothing to
invalidate. `OnStart` is **deferred to the entity's first Update**, so a test must
tick at least one frame before `GetNavMesh()` is non-null.

### Failure semantics

Every rejection fires `Zenith_Assert` **and** `Zenith_Error(LOG_CATEGORY_AI, …)`
**and** returns a defined failure. A malformed navmesh is a DEFECT, not an
expected state — the asset is committed to the repo, so absence or corruption
means a broken build.

| Situation | Result |
|---|---|
| empty path / missing file / unreadable file | `LoadFromFile` → `nullptr` |
| bad magic, wrong version, truncation, absurd count, out-of-range index, non-finite value | `ReadFromDataStream` → `false`, mesh left EMPTY |
| well-formed **zero-polygon** file | **VALID** — loads cleanly, no assert |
| bake with an empty path or empty geometry | asserts (caller defect) + typed error |
| bake whose generator finds nothing walkable | `GENERATION_FAILED`, logged, **no assert** (a DATA outcome) |

> Because the whole unit suite runs at boot and `Zenith_Assert` fires in every
> config, any unit that deliberately feeds corrupt data MUST wrap
> `Zenith_AssertCaptureScope` and assert both the hit count and the defined
> result. One unscoped deliberate assert kills the entire boot gate.

### Wire format and determinism

`"ZNAV"` v1, unchanged by the hardening — the exact layout is documented at the
top of `Zenith_NavMesh.h`. Two things to know before touching it:

- The per-polygon **center / normal / area are DEAD BYTES on the read path**:
  `BuildSpatialGrid` → `ComputeSpatialData` recomputes them from the vertices on
  every load. They are still written (churn-freedom) and read (to advance the
  cursor) but deliberately not validated — corrupting one is a no-op, which is
  worth remembering when a mutation test comes back inert. Mesh **bounds** ARE
  validated: `BuildSpatialGrid` early-outs on a zero-polygon mesh, so for an
  empty mesh the read bounds stand.
- **Determinism contract:** in-process re-bakes are byte-identical (the generator
  has no hashmap-iteration output dependence, no threading, no time or random
  source; the serializer is field-by-field). That is what lets a `.znavmesh` be
  committed to git without churning on every boot. Cross-toolchain byte equality
  is NOT promised — the drift check compares counts and bounds instead.

A `StreamEnvelope` header was deliberately REJECTED here: `AssetHandling` is
forbidden to the `ZenithAI` leaf, and `SentinelAI` enforces that.

### Editor debugging suite

The component's `RenderPropertiesPanel` (TOOLS only) is a full debugging surface,
not a ref field: editable asset ref with Apply/Reload/Unload; resolved path,
on-disk size and a wire-format header peek; a prominent
`LOADED / FAILED(reason) / UNLOADED` state; the whole `ComputeStats` block; six
visualisation toggles (edges, boundary edges, fill, adjacency links,
centers+normals, blocked highlight) driving `Zenith_NavMesh::DebugDraw(flags)`;
and two interactive probes — a **point probe**
(`IsPointOnNavMesh` / `ProjectPoint` / `FindNearestPolygon`) and a **path probe**
(`Zenith_Pathfinding::FindPath`, drawing the corridor and reporting length and
waypoint count). Draw state is per-component, so two meshes can be visualised
differently at once; the draw calls no-op under a Null render backend.

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
if (xResult.m_eStatus == Zenith_PathResult::Status::SUCCESS)
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
- Cell size: 5.0 units (fixed `m_fGridCellSize`, not configurable)

## Path Smoothing Algorithm

String-pulling removes unnecessary waypoints:

1. Start with raw A* path (polygon centers)
2. For each waypoint, check LOS to waypoints ahead
3. Skip intermediate waypoints if LOS exists
4. Results in smoother, shorter paths

## Debug Visualization

Two separate surfaces, with different granularity — do not look for the mesh
toggles in the global panel:

**The navmesh itself** is visualised PER COMPONENT, from
`Zenith_NavMeshComponent`'s editor panel: six flags (edges, boundary edges, fill,
adjacency links, centers+normals, blocked highlight) feed a
`Zenith_NavMeshDebugDrawFlags` straight into `Zenith_NavMesh::DebugDraw(flags)`.
Per-component is the point — two loaded navmeshes can be inspected differently at
once. There are deliberately **no** `AI/NavMesh/*` global debug variables; four
such flags existed for a while, were never read by anything, and were deleted.

**Agent paths** are global toggles in the debug-variable panel, under
`AI/Pathfinding` (backed by `Zenith_AIDebugVariables`):

- `AI/Pathfinding/Agent Paths` (`s_bDrawAgentPaths`): current path as lines
- `AI/Pathfinding/Path Waypoints` (`s_bDrawPathWaypoints`): waypoint markers

Both gate `Zenith_NavMeshAgent::DebugDraw`, which is driven once per frame from
`Zenith_AIAgentComponent::OnUpdate` (the component owns the nav agent, so there is
no registry of live agents to walk centrally). An agent wired outside an
`AIAgentComponent`, or one whose component is disabled, is not drawn.

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
