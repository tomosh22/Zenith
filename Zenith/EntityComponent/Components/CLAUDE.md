# Components

This directory contains all component types for the Entity-Component System.

## Existing Components

| Component | Purpose |
|-----------|---------|
| `Zenith_TransformComponent` | Position, rotation, scale (added automatically to all entities) |
| `Zenith_CameraComponent` | View/projection matrices for rendering |
| `Zenith_ModelComponent` | Renderable 3D mesh with materials (no animation - use AnimatorComponent) |
| `Zenith_TweenComponent` | Lightweight property tween system (animates position, scale, rotation over time) |
| `Zenith_AnimatorComponent` | Skeletal animation **forwarding handle** (auto-discovers skeleton from ModelComponent). The `Flux_AnimationController` lives in `Flux_AnimationControllerStore` (`g_xEngine.AnimationControllers()`), keyed by EntityID — see "AnimatorComponent is a forwarding handle" below |
| `Zenith_LightComponent` | Dynamic lights (directional, point, spot) |
| `Zenith_SunComponent` | Exactly-one scene sun authority (direction or time-of-day orbit only; solar colour/radiance derives in Flux) |
| `Zenith_AtmosphereComponent` | Scene-authored physical atmosphere medium (Rayleigh/Mie density scales, Mie-G phase asymmetry, both exponential scale heights, capture ground albedo) co-authored with a `Zenith_SunComponent` on one environment entity; resolved together via `Zenith_EnvironmentAuthorityData`. Also doubles as a **local blend volume** when `BlendRadius > 0` (see below). No radiometric anchor / exposure / renderer state (those are Flux-side) |
| `Zenith_ColliderComponent` | Physics collision shapes (Jolt integration) |
| `Zenith_TerrainComponent` | Heightmap-based terrain with streaming, plus a TIER 1 **collision**-surface height query (`TryGetGroundHeightAt` — 4 m quads, NOT the rendered ground) — see "Terrain ground-height query" below |
| `Zenith_InstancedMeshComponent` | GPU-instanced mesh rendering, plus an optional **per-instance collider** — one static Jolt capsule per live instance, owned by the component (see below) |
| `Zenith_ParticleEmitterComponent` | Particle effect emitters |
| `Zenith_GraphComponent` | Behaviour Graph host (multiple .bgraph slots per entity, hot-reloadable) |
| `Zenith_UIComponent` | UI element support |
| `Zenith_AIAgentComponent` | AI integration seams: perception registration + a `Zenith_NavMeshAgent` that is either **borrowed** or **owned** (see below). Decision-making is graphs, not a behaviour tree |
| `Zenith_AttachmentComponent` | Bone-attachment that follows a named bone on another entity each frame (e.g. racket in hand, held weapon) |
| `Zenith_NavMeshComponent` | Baked-navmesh holder — loads a committed `.znavmesh` in `OnStart` and owns it for the component's lifetime; rich TOOLS debugging panel |

## Terrain ground-height query: TIER 1 landed, TIER 2 still deferred (task_0515a49e / Zenithmon Shortfalls E8 / ZM-49)

`Zenith_TerrainComponent` answers "what is the **collision** surface height at
world XZ?" directly:
`[[nodiscard]] bool TryGetGroundHeightAt(fWorldX, fWorldZ, fHeightOut) const`,
plus two static cores usable without a live component —
`TryGetGroundHeightFromGeometry(const Flux_MeshGeometry*, …)` and the Flux-free
`TryGetGroundHeightFromTriangles(positions, vertexCount, indices, indexCount, …)`.
It scans the terrain's combined physics geometry — the same data
`HasPhysicsGeometry()` / `GetPhysicsMeshGeometry()` expose — for the triangle
whose XZ projection contains the point and interpolates **barycentrically**, so
an interior point gets the exact plane height rather than a nearest-vertex
approximation. No Jolt, no body filtering; winding-agnostic (divides by the
*signed* projected area, so it does not depend on which way a mesh's triangles
wind).

> **★ IT IS NOT THE RENDERED GROUND, AND THAT IS NOT A ROUNDING ERROR — so it is
> NOT a general placement query.** The physics mesh is baked at **4 m** quads and
> the HIGH render mesh at **1 m** quads
> (`Zenith_TerrainChunkLayout::uPHYSICS_CHUNK_VERTEX_COUNT ==
> CalculateChunkVertexCount(4u)` against `uHIGH_CHUNK_VERTEX_COUNT ==
> CalculateChunkVertexCount(1u)`), and that header says the gap is deliberate:
> *"collision deliberately remains lower density than the nearby HIGH render
> mesh"*. The answer is therefore the **4 m chord** across the visible surface —
> **below** it over a convex ridge, **above** it in a concave dip, error bounded
> by local relief over 4 m — so an object placed at the returned height sinks
> into or floats over the terrain the player sees. Use it where the question is
> about physics (where a body rests, whether a capsule clears a step); a question
> about a **visible** gap — ZM-D-173's door-jamb residual is exactly one — needs
> TIER 2, not this.
>
> **Positions are read RAW.** The lookup never applies the entity's transform, and
> neither does `Zenith_ColliderComponent::CreateTerrainShape` when it feeds Jolt
> the same triangles — but the *body* is created at the entity's
> `GetPosition()`/`GetRotation()`. The query agrees with physics and with the
> renderer only while the terrain entity is identity. True of every terrain
> authored today; nothing enforces it.

**Why this exists instead of a physics raycast, and why reaching for one
instead would reintroduce the occlusion problem it exists to avoid:** a raycast
answers a materially different question — *what is the first BODY below this
point* — and anything standing on the ground occludes it. That is not a corner
case, and it is not filterable:

- `Zenith_Physics::Raycast` / `Zenith_PhysicsQuery::RaycastIgnoring` take exactly
  **one** ignore entity, so two overlapping bodies over a column are unmeasurable;
- restarting the ray below a hit does not rescue it: an object *standing* on the
  ground has its underside AT the surface, and anything deliberately embedded (a
  shell sunk 0.05 m so no visible gap opens) has it BELOW the surface, so the
  restart begins underneath the terrain.

The query reads the terrain's own **collision** surface instead of a body, so the
occlusion problem is gone outright for it — that is the whole reason it exists.
(It buys no accuracy against the rendered surface; see the box above.)

**Failure contract — THREE cases, not two.** All three return `false` and leave
`fHeightOut` UNTOUCHED, so a caller can never mistake any of them for a genuine
ground height of 0.0 (and a real 0.0 still arrives with `true`):

1. **absent** physics geometry (every chunk's source mesh failed to load);
2. an XZ **outside** the combined mesh's footprint;
3. an XZ over a **hole inside** that footprint. `CombineTerrainChunkGridCore`
   skips any chunk whose source mesh fails to load (recording it in
   `TerrainSparseLoadDiagnostics`, warned by `LogSparseLoadDiagnostics`), so a
   sparse or partial bake leaves gaps in the combined mesh — and this API cannot
   tell a probe over one from case 2. **A caller taught "false means outside the
   world" will read a sparse bake as a smaller world.**

`[[nodiscard]]` makes ignoring the bool a compiler diagnostic, not a silent zero.

**Multi-hit policy lives on `TryGetGroundHeightFromTriangles`, and it has a
precondition.** The **first containing triangle in index order** wins. That is
safe for a *heightfield* — at most one triangle per XZ column, and the only
doubly-contained points are on a shared edge or vertex where both triangles
interpolate the same shared vertices and agree. For an arbitrary soup with an
**overhang** the premise is false and you get whatever the index buffer lists
first; index order is not a defensible *ground* policy (highest-below would be),
and this core neither implements that nor detects the case.

**What TIER 1 does NOT buy:**

- it answers the **collision** surface, so it cannot answer a question about the
  rendered one (see the box above);
- it needs `m_pxPhysicsGeometry` populated, which only `LoadCombinedPhysicsGeometry`
  fills. A scene **load** fills it (`ReadFromDataStream` calls it), so a runtime
  probe on a terrain loaded from a scene is answered — but the editor's
  **Add-Component** path constructs through the deserialization ctor without
  ever deserializing, so a probe on a freshly-added terrain still MISSES;
- it has **one level** of spatial index and no more (ZEN-2). `TryGetGroundHeightAt`
  first rejects against each combined *physics chunk*'s own XZ bounds — a
  per-chunk table (`PhysicsChunkSpan` / `m_pxPhysicsChunkSpans`) built ONCE when
  the physics geometry is combined, never per call — and then runs the unchanged
  linear per-triangle scan restricted to the one chunk (or, exactly at a shared
  seam, two) whose bounds contain the point. So the cost is
  O(chunks) + O(triangles in the matching chunk) — roughly one float-compare
  reject per chunk in the ACTIVE grid (up to 4096, and far fewer on a terrain
  smaller than the fixed slot capacity) plus ~512 triangles, rather than up to
  ~2.1M triangles. Still no
  spatial index *within* a chunk, and still no allocation per query. Good enough
  for one-off probes, placement, and occasional per-agent use; a per-frame,
  per-agent query over a large terrain would want TIER 2's heightfield, not a
  third level of this. **The two static entry points
  (`TryGetGroundHeightFromGeometry` / `TryGetGroundHeightFromTriangles`) stay
  unaccelerated by design** — they have no owning component to hold a table, so a
  caller reaching them directly still pays the full per-triangle scan.
- **The per-chunk table's offsets are OBSERVED, not computed from a grid
  coordinate**, and that is load-bearing rather than stylistic: a sparse bake
  reserves nothing for a chunk that failed to load, so `(x, y) -> index run` is
  not a formula. See `PhysicsChunkSpan`'s doc comment in
  `Zenith_TerrainComponent.h`.
- the per-*triangle* XZ box underneath all of that is **not purely an
  accelerator**: the barycentric test alone admits a ~1e-5-of-a-triangle band
  outside it (the tolerance that keeps a point on an interior edge from falling
  down a zero-width crack) and the box *clips* it. Desirable — it is what stops
  the tolerance extrapolating a height off the rim of the mesh — but it changes
  the answer at a boundary rather than merely speeding the scan up. The
  per-chunk reject above it cannot change any answer: a triangle's bounds are
  always a subset of its own chunk's, so a rejected chunk provably held no
  containing triangle.

**TIER 2** — keep or load the heightfield — is still DEFERRED, and it now has
**two** jobs: make the query answer at **authoring time**, and make it answer for
the **rendered** surface (the heightfield is what both densities are sampled
from, so it carries no 4 m chord error). This component holds **no** height data
at runtime even now: it loads baked mesh chunks, and
`Terrain/<Set>/Height.ztxtr` is read only by the TOOLS editor path.

Full mechanics are in the doc comment above `TryGetGroundHeightAt` in
`Zenith_TerrainComponent.h`; the caller that motivated this work (ZM-D-173's
Home door jambs — whose residual is a **visual** one and so is **not** measurable
by TIER 1, per the box above) and the TIER 2 write-up are in
`Games/Zenithmon/Docs/Shortfalls.md` section 2 (E8).

## Instance colliders: one static body per live instance, owned by the component

`Zenith_InstancedMeshComponent` can declare a per-instance collider. Enabling it gives
**every live instance one static Jolt capsule** — RenderTest's `TerrainTrees_Trunk` is
2520 of them — created from the component's authored config and destroyed with the
instance. It is what makes the player collide with instanced trees instead of walking
through them.

### The config, and what it costs when it is NONE

```cpp
enum InstanceColliderType : uint32_t { INSTANCE_COLLIDER_TYPE_NONE = 0, INSTANCE_COLLIDER_TYPE_CAPSULE = 1 };

struct Zenith_InstanceColliderConfig
{
    InstanceColliderType m_eType = INSTANCE_COLLIDER_TYPE_NONE;
    float m_fRadius = 0.3f;              // local (pre-scale)
    float m_fCylinderHalfHeight = 3.2f;  // Jolt convention: total half-extent = this + radius
    float m_fLocalYOffset = 3.5f;        // capsule centre above the instance origin (pre-scale)
};
```

`NONE` is the default and is the whole compatibility story: every hook below
early-returns on a single enum compare, so an instanced-mesh user that never enables a
collider behaves exactly as it did — no physics access, no bodies, no per-frame cost.
The one thing that does change for it is the stream: a NONE component still WRITES the
four values (type 0 plus the three defaults), so every serialized instanced-mesh
component grows by 16 bytes whether or not it uses the feature.

Public surface: `SetInstanceColliderCapsule(radius, cylHalfHeight, localYOffset)` (creates
bodies for every currently-enabled instance AND every one spawned after; idempotent for an
identical config), `ClearInstanceColliderConfig()`, `GetInstanceColliderConfig()`,
`HasInstanceColliders()`, `GetInstanceBodyCount()`, `GetInstanceBodyID(slot)`.

### The slot ledger

`Zenith_Vector<Zenith_PhysicsBodyID> m_axInstanceBodyIDs`, indexed **by instance slot**,
INVALID meaning "no body for that slot". Slot-indexed rather than packed because
`Flux_InstanceGroup::RemoveInstance` is disable-in-place + free-list reuse, not
swap-and-pop: a live instance's ID never changes (which is what makes keying external
state by slot legal at all), but live slots are **not contiguous** after any removal.

`DestroyAllInstanceBodies` sweeps the LEDGER, never `ComputeVisibleIndices` —
`Flux_InstanceGroup::Clear()` zeroes the counts and free list but leaves per-slot flags
set, so the enabled-slot list can disagree with reality after one. The ledger cannot.

★ **The CREATE sweep has no such escape** — after a `Clear()` the ledger is empty, so the
group is the only source of truth left — so it carries a named `Zenith_Assert` instead:
an enabled list LONGER than `GetInstanceCount()` is that stale-flag state and nothing
else. Left unguarded, a config set after a bare `Clear()` would build one body per stale
flag (invisible walls at deleted instances' transforms) and the occupied-slot early-return
would then deny a real respawned instance its collider. **The root fix belongs in
`Flux_InstanceGroup::Clear()`** — zeroing per-slot flags there, matching `RemoveInstance` —
which would also stop the same stale list reaching `WriteToDataStream` and
`UpdateGPUBuffers`, where it can already render and serialize ghost instances. That is a
pre-existing Flux defect with its own blast radius (CityBuilder is the other `Clear()`
caller) and is deliberately NOT fixed here; the assert is what stops it reaching physics
meanwhile.

### Every mutation entry point is hooked

| Site | Hook |
|---|---|
| `SpawnInstance` / `SpawnInstanceWithMatrix` | `CreateInstanceBody(slot)`, AFTER the transform is set (the body pose is decomposed from it) |
| `DespawnInstance` | `DestroyInstanceBody(slot)` **before** `RemoveInstance` — the slot is about to be free-listed and must not be inherited with a body |
| `ClearInstances` | `DestroyAllInstanceBodies()` before `Clear()` |
| `SetInstanceTransform` / `SetInstanceMatrix` | `RefreshInstanceBody(slot)` = destroy + recreate (the capsule's dimensions are scale-derived, so a transform change can move the shape too) |
| `SetInstanceEnabled` | create on true, destroy on false. `CreateInstanceBody` early-RETURNS on an occupied ledger slot rather than asserting, because enabling an already-enabled instance is legal here |

**`CreateInstanceBody` refuses a DEAD slot**, and that is what keeps the two creation
paths agreeing on what "live instance" means. `CreateAllInstanceBodies` filters through
the group's enabled-slot list; the per-slot path reads the same `m_uFlags` before
building anything. Without that check, `SetInstanceTransform` on a slot that
`SetInstanceEnabled(false)` had just disabled would RESURRECT its collider — an
invisible instance that still blocks the player, and one `WriteToDataStream` (visible
slots only) would not serialize, so it would exist in the authoring session and vanish
on reload. Pinned by `InstancedMesh, RefreshDoesNotResurrectDisabledInstanceBody`.
| destructor | `DestroyAllInstanceBodies()` FIRST, through `Zenith_Physics::TryGet()` — pool teardown can run after `Physics::Shutdown` |
| move ctor / move assign | both new members transfer, and the SOURCE ledger is emptied; move-assign destroys its OWN bodies first. A shared ledger means the moved-from destructor takes bodies the target now owns (the `7fd3ccdc` move-op regression class) |

**Reconfiguring goes through `RebuildInstanceBodies()`** — destroy-all, create-all, then
`Zenith_Physics::OptimizeBroadPhase()` past 64 bodies, exactly as the deserialize path
does. It is the second bulk-add site and needs the same broadphase re-optimise; skipping
it there was an asymmetry rather than a decision. `SetInstanceColliderCapsule` short-circuits
entirely when the four authored values are unchanged AND bodies already exist, so a
re-call (the tree authoring and the editor panel both re-call) costs nothing and does not
recycle body ids. Pinned by `InstancedMesh, ReconfigureRebuildsWithNewDimensions`.

★ **The editor panel applies on RELEASE, not per drag frame** (`ImGui::IsItemDeactivatedAfterEdit`).
A live-apply drag would destroy and recreate one Jolt body per instance per frame — 2520 for
RenderTest's trunk group — and the editor's Stopped mode never calls `PhysicsSystem::Update`
(`Zenith_Core.cpp` gates it on Playing), so nothing reclaims the broadphase nodes that churn
allocates. The comparable destructive rebuild on `Zenith_ColliderComponent`'s panel sits
behind a button for the same reason.

Body pose: `Zenith_Maths::DecomposeTRS` on the instance matrix, then
`position + rotation * (0, localYOffset * scale.y, 0)`, radius `* max(|scale.x|, |scale.z|)`
(a capsule has one radius, so a non-uniform XZ scale must collapse to a scalar) and cylinder
half-height `* |scale.y|`.

### Serialization: v4 -> v5

The stream gained four values, written **after** `m_bAnimationsPaused` and **before** the
instance count: `uint32 type`, `float radius`, `float cylHalfHeight`, `float localYOffset`
(+16 bytes per serialized component). The read is gated on `uVersion >= 5`, so a v4 stream
skips the block and the config stays NONE — byte-for-byte the old behaviour. The config is
written before the instance data deliberately: the read path needs it in hand by the time
`SpawnInstanceWithMatrix` starts creating bodies inline. After the spawn loop, a
deserialize that created more than 64 bodies calls
`Zenith_Physics::OptimizeBroadPhase()` (2520 one-at-a-time static adds otherwise leave
Jolt's quadtree unoptimised until simulation steps run).

Only `Games/RenderTest/Assets/Scenes/RenderTest.zscen` carries InstancedMesh component
data among the committed scenes, and RenderTest re-authors + republishes it on every tools
boot, so the version bump self-publishes.

### Deliberate v1 exclusions

These follow from "no component per body", and each is a real limitation rather than an
oversight:

- instance bodies are invisible to `Zenith_SyncPhysicsTransforms` (nothing writes their
  pose back — correct, they are static), to `Zenith_AINavGeometry` (they obstruct no
  navmesh; instanced trees never did) and to `Zenith_PhysicsDebugDraw`;
- a contact or raycast attributes to the **group** entity, not to an individual instance.
  `OnCollisionEnter/Stay/Exit` fire with the group's entity;
- a 128K-instance group must **not** enable a collider config —
  `Zenith_Physics::s_uMaxBodies` is 65536.

Pinned by the `InstancedMesh, *` units in
`EntityComponent/Zenith_InstancedMeshComponent.Tests.inl` (config sweeps, slot recycling,
pose + shape, v5 round-trip, v4 compatibility, both move ops) and end-to-end by
RenderTest's `RT_TreeCollision`.

## AIAgentComponent's nav agent: borrowed OR owned, via two pointers

```cpp
Zenith_NavMeshAgent* m_pxNavMeshAgent      = nullptr;  // ACTIVE: borrowed or owned
Zenith_NavMeshAgent* m_pxOwnedNavMeshAgent = nullptr;  // non-null only when WE allocated
// INVARIANT: m_pxOwnedNavMeshAgent != nullptr  =>  m_pxOwnedNavMeshAgent == m_pxNavMeshAgent
```

**Two pointers, not a pointer plus an ownership bool.** The bool form makes the
self-assignment and borrow-vs-own cases easy to get subtly wrong and gives you
nothing to assert; this shape has one checkable invariant, and
`ReleaseOwnedNavMeshAgent()` is the single place the owned agent is deleted, so
the destructor, `SetNavMeshAgent` and both move operations cannot disagree.

| Operation | Contract |
|---|---|
| `EnsureOwnedNavMeshAgent()` | **Preserves an existing borrow** — a game's explicit wiring is a decision an auto-wire must never replace. Otherwise allocates into both pointers. Idempotent. |
| `SetNavMeshAgent(p)` | `p == the installed pointer` is a **no-op** — the case that bites, since "free the old one then assign" would delete the very agent being installed. `nullptr` clears both. Anything else frees an owned agent, then installs the borrow. |
| destructor | frees the owned agent only |
| move-construct / move-assign | both pointers transfer and the **source is neutralised**; move-assign frees its own agent first. A source left holding the owned pointer would free it out from under the target — the pool move-constructs then destructs the source. |

**Neither pointer is serialized**, so this added no `.zscen` format change and no
cross-game scene republish. Nothing auto-creates an agent: one appears only when
something asks. The graph-side asker is the **`EnsureNavAgent`** node — see
`../../AI/Navigation/CLAUDE.md`.

## AnimatorComponent is a forwarding handle (Wave-19 ownership relocation)

`Zenith_AnimatorComponent` does **not** own a `Flux_AnimationController` by value any more. The controller lives in an owning Flux subsystem — `Flux_AnimationControllerStore` (`Flux/MeshAnimation/Flux_AnimationControllerStore.{h,cpp}`), reached via `g_xEngine.AnimationControllers()` — keyed by the entity's **stable `Zenith_EntityID` slot**. The component is a thin handle: every public accessor (`GetController`, `GetStateMachine`, `SetFloat`, `CrossFade`, `AddClipFromFile`, serialization, the editor panel, …) forwards into the store-owned controller. This is the ECS-side twin of WS18's `Zenith_TerrainComponent` → `Flux_TerrainStreamingState` relocation, and it lets `Zenith_AnimatorComponent.h` carry **zero** Flux includes (the heavy `Flux_AnimationController.h` header edge — old allowlist line 20 — is gone; the forwarding bodies' Flux includes live in the `.cpp`, which is allow-listed).

Key invariants (pinned by the `Animator` regression suite in `Core/Zenith_UnitTests.Tests.inl`):

- **Heap-stable storage.** The store allocates each `Flux_AnimationController` with `new` (`Zenith_Vector<Flux_AnimationController*>` + an index-by-entity-slot `Zenith_Vector<u_int>` for O(1) lookup). The pointer never moves, so the component's cached `m_pxController` (and any game code caching `Flux_AnimationLayer*` / `Flux_AnimationStateMachine*` into the controller's sub-objects) survives a component-pool relocation (swap-and-pop / `Grow`) **and** a cross-scene `MoveEntityToScene`.
- **Hot path is O(1), no hash.** `OnUpdate` dereferences the cached `m_pxController` directly — no per-frame store lookup. The ctor primes the cache (`GetOrCreate`); `OnStart` re-primes it.
- **Exactly one controller per entity, exactly one Destroy.** `Destroy(EntityID)` is idempotent. Both the component dtor and `OnDestroy` call it (whichever fires first does the work; the second is a no-op). A **moved-from** component is neutralised (`m_bMovedOut = true`, cached pointer nulled) so the pool's move-construct-then-destruct-source sequence never double-frees — the moved-to instance shares the same EntityID-keyed controller.
- **`GetCurrentAnimatorStateInfo()` returns `Zenith_AnimatorStateInfo`** — an EC-side mirror POD of `Flux_AnimatorStateInfo` (same field names/types + `IsName`). It is implicitly convertible to `Flux_AnimatorStateInfo` (operator defined in the `.cpp`), so callers that include the Flux state-machine header keep compiling unchanged. The mirror is what lets the by-value return stay Flux-include-free in the header.
- **Render path is unaffected.** Skinning matrices are read from `Zenith_ModelComponent::GetSkeletonInstance()->GetSkinningMatrices()` by the unified compute-skinning path, never from the controller. Relocating the controller's *ownership* cannot regress rendering. Serialization byte-format is unchanged (no `.zscen` / `.zprfb` bump).

## Environment authority: one global Sun/Atmosphere + local blend volumes

The environment resolves in **two layers** (Unity's Volume model, minus the parts
that do not apply):

- **BASE** — exactly one *global* environment entity (`BlendRadius == 0`), chosen by
  active-scene-first then lowest stable entity ID. **More than one global is a
  conflict**, and in a `ZENITH_TOOLS` build it is now a hard `Zenith_Assert`, not
  just a log line — it is silent data loss (the loser's Sun/Atmosphere is dropped
  and the scene renders as though it were never authored). The Sun and Atmosphere
  property panels also paint a red banner on any losing entity. Tests that build a
  conflict on purpose scope the assert off with
  `Zenith_ScopedEnvironmentConflictAssertSuppression`.
- **LOCAL** — any number of `Zenith_AtmosphereComponent`s with `BlendRadius > 0`.
  Each is a sphere around its own transform; its weight falls from 1 inside
  `radius - falloff` to 0 at `radius`, evaluated at the **view position** (taken
  from the neutral render gather, so it is exactly the camera the renderer draws
  from). They are applied in ascending `(BlendPriority, entity ID)` and each LERPs
  the accumulated medium toward its own values — so overlapping volumes compose
  predictably and the result never depends on ECS query order. A local volume
  never competes for authority and never conflicts.

**The Sun is deliberately never blended.** It is a celestial object: its direction
cannot depend on where the camera stands, and making it do so would break both the
shadow fit and the direct↔ambient agreement the system is built on. Only the medium
is local — a dusty basin, a humid valley, a smoggy district.

A scene with no local volumes resolves bit-identically to the single-winner rule
that predates this, which is why the feature is safe on by default. Pure helpers
(`Zenith_ComputeBlendVolumeWeight`, `Zenith_BlendAtmosphereLayer`) live in
`Core/Zenith_EnvironmentAuthority.h` and are unit-tested directly.

## Creating a New Component

### Required Structure

1. **Constructor** accepting `Zenith_Entity&`:
   ```cpp
   Zenith_MyComponent(Zenith_Entity& xParent);
   ```

2. **Serialization methods**:
   ```cpp
   void WriteToDataStream(Zenith_DataStream& xStream);
   void ReadFromDataStream(Zenith_DataStream& xStream);
   ```

3. **Editor panel** (when `ZENITH_TOOLS` defined):
   ```cpp
   void RenderPropertiesPanel();
   ```
   Draw ONLY the component's rows. The Properties panel owns the framed
   section header (icon, name, remove button) and records undo for every
   edit made inside the body, so a panel must not call `CollapsingHeader`
   for itself or touch the undo system. Rows that edit a Vector3 should use
   `Zenith_ImGuiWidgets::Vec3Field` (Core; the colour-tagged X/Y/Z row the
   Transform uses) rather than a bare `DragFloat3`.

### Registration

In the component's .cpp file, add the registration macro after includes:

```cpp
ZENITH_REGISTER_COMPONENT(Zenith_MyComponent, "MyComponent")
```

This registers the component type with the ComponentMeta system, enabling:
- Type-erased create/remove operations
- Automatic serialization via the meta registry
- Lifecycle hook detection and dispatch
- Automatic editor registration (component appears in "Add Component" menu)

### Optional Lifecycle Hooks

Components can implement any subset of these hooks (detected at compile-time):

```cpp
void OnAwake();           // Called when component is created
void OnStart();           // Called before first update
void OnEnable();          // Called when component is enabled
void OnDisable();         // Called when component is disabled
void OnUpdate(float fDt); // Called every frame
void OnLateUpdate(float fDt);  // Called after all OnUpdate
void OnFixedUpdate(float fDt); // Called at fixed timestep
void OnDestroy();         // Called before component removal
```

If a hook is not implemented, it's simply skipped during dispatch.

Additional collision hooks are concept-detected by the same compile-time meta system, dispatched by components handling physics interactions (e.g. `Zenith_GraphComponent` fires these to its behaviour graphs):

```cpp
void OnCollisionEnter(Zenith_Entity xOther);   // First frame of contact
void OnCollisionStay(Zenith_Entity xOther);    // While contact persists
void OnCollisionExit(Zenith_EntityID xOtherID); // Contact ended
```

## Component Concept

Components must satisfy the `Zenith_Component` concept defined in `Zenith_Scene.h`:
- Constructible from `Zenith_Entity&`
- Destructible
- In ZENITH_TOOLS builds: must have `RenderPropertiesPanel()`

## Serialization Order

Component serialization order is determined by the explicit `order` argument passed to `RegisterComponent<T>(name, order)` at registration time (`Zenith_ComponentMeta_Registration.cpp` for the built-ins). Lower values serialize first. This matters for dependencies (e.g., TerrainComponent must serialize before ColliderComponent that depends on terrain data).

Current order (centralised in `Zenith_ComponentMeta_Registration.cpp`):
Transform (0), Model (10), Tween (12), Animator (15), Camera (20), Light (25), Sun (26),
Atmosphere (27),
Terrain (40), Collider (50), Graph (60), UI (70), InstancedMesh (80),
ParticleEmitter (85), AIAgent (90), Attachment (95), NavMesh (96).

New components default to order 1000 (serialized last). Game components use
orders 100+ (unique per game).

## Accessing Components

```cpp
// Add component to entity
entity.AddComponent<Zenith_MyComponent>();

// Check if entity has component
if (entity.HasComponent<Zenith_MyComponent>()) { ... }

// Get component reference
auto& component = entity.GetComponent<Zenith_MyComponent>();

// Remove component
entity.RemoveComponent<Zenith_MyComponent>();
```

### Guarded reads: prefer `TryGetComponent<T>()`

For the common "use it only if present" pattern, use the single-lookup
`TryGetComponent<T>()` (returns `T*`, or `nullptr` if absent) rather than the
`HasComponent<T>()` + `GetComponent<T>()` double lookup:

```cpp
// CANONICAL guarded read — one pool lookup, and it additionally short-circuits
// to nullptr on an unloaded / stale scene (a safety property Has+Get lack).
if (Zenith_MyComponent* pxC = entity.TryGetComponent<Zenith_MyComponent>())
{
    pxC->DoThing();
}

// AVOID — two pool lookups, and GetComponent asserts at the unload edge.
if (entity.HasComponent<Zenith_MyComponent>())
{
    entity.GetComponent<Zenith_MyComponent>().DoThing();
}
```

Keep `HasComponent<T>()` for pure presence checks, and keep an explicit
`Zenith_Assert(entity.HasComponent<T>()); entity.GetComponent<T>()...` where the
component is a hard precondition (an intentional assert — do NOT silently convert
those to a `TryGetComponent` guard).

## Querying Multiple Components

Use the Query system to iterate entities with specific component combinations.
There are three query scopes on `g_xEngine.Scenes()` — pick by which scenes you
mean; none requires a raw slot loop (the slot accessors are internal):

```cpp
// Active scene only:
g_xEngine.Scenes().QueryActiveScene<TransformComponent, ColliderComponent>()
    .ForEach([](Zenith_EntityID id, TransformComponent& t, ColliderComponent& c) {
        // Process entities with both components in the active scene
    });

// Every loaded scene:
g_xEngine.Scenes().QueryAllScenes<TransformComponent, ColliderComponent>()
    .ForEach([](Zenith_EntityID id, TransformComponent& t, ColliderComponent& c) {
        // Process entities with both components across all loaded scenes
    });

// One specific scene you already hold a SceneData* for (e.g. via
// GetActiveSceneData() / GetSceneDataForEntity(id)):
pxSceneData->Query<TransformComponent, ColliderComponent>()
    .ForEach([](Zenith_EntityID id, TransformComponent& t, ColliderComponent& c) { /* ... */ });
```

All four query forms expose `ForEach` / `Count` / `First` / `Any`.
