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
| `Zenith_ColliderComponent` | Physics collision shapes (Jolt integration) |
| `Zenith_TerrainComponent` | Heightmap-based terrain with streaming |
| `Zenith_InstancedMeshComponent` | GPU-instanced mesh rendering |
| `Zenith_ParticleEmitterComponent` | Particle effect emitters |
| `Zenith_GraphComponent` | Behaviour Graph host (multiple .bgraph slots per entity, hot-reloadable) |
| `Zenith_UIComponent` | UI element support |
| `Zenith_AIAgentComponent` | Behaviour tree execution + blackboard state (AI system integration) |
| `Zenith_AttachmentComponent` | Bone-attachment that follows a named bone on another entity each frame (e.g. racket in hand, held weapon) |

## AnimatorComponent is a forwarding handle (Wave-19 ownership relocation)

`Zenith_AnimatorComponent` does **not** own a `Flux_AnimationController` by value any more. The controller lives in an owning Flux subsystem — `Flux_AnimationControllerStore` (`Flux/MeshAnimation/Flux_AnimationControllerStore.{h,cpp}`), reached via `g_xEngine.AnimationControllers()` — keyed by the entity's **stable `Zenith_EntityID` slot**. The component is a thin handle: every public accessor (`GetController`, `GetStateMachine`, `SetFloat`, `CrossFade`, `AddClipFromFile`, serialization, the editor panel, …) forwards into the store-owned controller. This is the ECS-side twin of WS18's `Zenith_TerrainComponent` → `Flux_TerrainStreamingState` relocation, and it lets `Zenith_AnimatorComponent.h` carry **zero** Flux includes (the heavy `Flux_AnimationController.h` header edge — old allowlist line 20 — is gone; the forwarding bodies' Flux includes live in the `.cpp`, which is allow-listed).

Key invariants (pinned by the `Animator` regression suite in `Core/Zenith_UnitTests.Tests.inl`):

- **Heap-stable storage.** The store allocates each `Flux_AnimationController` with `new` (`Zenith_Vector<Flux_AnimationController*>` + an index-by-entity-slot `Zenith_Vector<u_int>` for O(1) lookup). The pointer never moves, so the component's cached `m_pxController` (and any game code caching `Flux_AnimationLayer*` / `Flux_AnimationStateMachine*` into the controller's sub-objects) survives a component-pool relocation (swap-and-pop / `Grow`) **and** a cross-scene `MoveEntityToScene`.
- **Hot path is O(1), no hash.** `OnUpdate` dereferences the cached `m_pxController` directly — no per-frame store lookup. The ctor primes the cache (`GetOrCreate`); `OnStart` re-primes it.
- **Exactly one controller per entity, exactly one Destroy.** `Destroy(EntityID)` is idempotent. Both the component dtor and `OnDestroy` call it (whichever fires first does the work; the second is a no-op). A **moved-from** component is neutralised (`m_bMovedOut = true`, cached pointer nulled) so the pool's move-construct-then-destruct-source sequence never double-frees — the moved-to instance shares the same EntityID-keyed controller.
- **`GetCurrentAnimatorStateInfo()` returns `Zenith_AnimatorStateInfo`** — an EC-side mirror POD of `Flux_AnimatorStateInfo` (same field names/types + `IsName`). It is implicitly convertible to `Flux_AnimatorStateInfo` (operator defined in the `.cpp`), so callers that include the Flux state-machine header keep compiling unchanged. The mirror is what lets the by-value return stay Flux-include-free in the header.
- **Render path is unaffected.** Skinning matrices are read from `Zenith_ModelComponent::GetSkeletonInstance()->GetSkinningMatrices()` by the unified compute-skinning path, never from the controller. Relocating the controller's *ownership* cannot regress rendering. Serialization byte-format is unchanged (no `.zscen` / `.zprfb` bump).

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
Transform (0), Model (10), Tween (12), Animator (15), Camera (20), Light (25),
Terrain (40), Collider (50), Graph (60), UI (70), InstancedMesh (80),
ParticleEmitter (85), AIAgent (90), Attachment (95).

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
