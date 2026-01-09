# Entity-Component System

## Files

### Core
- `Zenith_Scene.h/cpp` - Scene management, component pools, entity-component mapping
- `Zenith_Entity.h/cpp` - Entity wrapper class (scene pointer + entity ID + parent ID + child entity vector)
- `Zenith_ComponentMeta.h/cpp` - Component reflection/registration system with type-erased operations
- `Zenith_ComponentRegistry.h/cpp` - Component type registration for editor UI
- `Zenith_Query.h` - Multi-component entity queries
- `Zenith_EventSystem.h/cpp` - Type-safe event dispatcher with deferred queue

### Components (in Components/ subdirectory)
- `Zenith_TransformComponent` - Position, rotation, scale
- `Zenith_CameraComponent` - View/projection matrices
- `Zenith_ModelComponent` - Renderable mesh with material
- `Zenith_ColliderComponent` - Physics collision shapes (Jolt Physics)
- `Zenith_TerrainComponent` - Heightmap-based terrain
- `Zenith_ScriptComponent` - Custom behavior attachment
- `Zenith_TextComponent` - 2D text rendering
- `Zenith_UIComponent` - UI element support

## Architecture

### Entity
Wrapper around scene pointer, entity ID, parent entity ID, and child entity ID vector. Entity names are stored in the scene (not the entity) via `GetName()`/`SetName()` accessors that delegate to the scene.

### Scene
Static singleton storing all entities and component pools. Components of same type stored in contiguous `Zenith_Vector` for cache-friendly iteration. Scene serialization uses `Zenith_DataStream` with `.zscn` file extension.

### Component Pools
Each component type has a dedicated pool with:
- `m_xData` - Vector of component instances
- `m_xOwningEntities` - Parallel vector tracking which entity owns each component

**Swap-and-Pop Removal:** When a component is removed, it's swapped with the last element in the pool, then the last element is removed. This avoids expensive array shifts but means component indices are unstable after removal. The moved component's owner index is updated in `m_xEntityComponents`.

## Component Meta System

The `Zenith_ComponentMeta` system provides type-erased component operations using function pointers (no std::function for performance).

### Registering Components
Components are registered using the `ZENITH_REGISTER_COMPONENT` macro in their .cpp file:

```cpp
ZENITH_REGISTER_COMPONENT(Zenith_TransformComponent, "TransformComponent")
```

The macro creates a static auto-registrar that registers the component type at startup. Serialization order is determined automatically by component type name.

### Type-Erased Operations
Each registered component type has function pointers for:
- Create, Has, Remove operations
- Serialize/Deserialize via DataStream
- Lifecycle hooks (OnAwake, OnStart, OnUpdate, etc.) if implemented

## Lifecycle Hooks

Components can optionally implement lifecycle hooks detected at compile-time using C++20 concepts:

| Hook | Signature | When Called |
|------|-----------|-------------|
| `OnAwake()` | `void OnAwake()` | When component is created |
| `OnStart()` | `void OnStart()` | Before first update |
| `OnEnable()` | `void OnEnable()` | When component is enabled |
| `OnDisable()` | `void OnDisable()` | When component is disabled |
| `OnUpdate(float)` | `void OnUpdate(float fDt)` | Every frame |
| `OnLateUpdate(float)` | `void OnLateUpdate(float fDt)` | After all OnUpdate calls |
| `OnFixedUpdate(float)` | `void OnFixedUpdate(float fDt)` | At fixed timestep (physics) |
| `OnDestroy()` | `void OnDestroy()` | Before component removal |

Hooks are optional - if a component doesn't implement a hook, the corresponding function pointer is null and skipped during dispatch. Dispatch happens via `Zenith_ComponentMetaRegistry::DispatchOn*()` methods.

## Query System

Multi-component queries allow iterating entities that have specific component combinations:

```cpp
scene.Query<TransformComponent, ColliderComponent>()
    .ForEach([](Zenith_EntityID uID, TransformComponent& xT, ColliderComponent& xC) {
        // Process entities with both components
    });
```

Query methods:
- `ForEach(callback)` - Iterate all matching entities
- `Count()` - Return number of matching entities
- `First()` - Return first matching entity ID (or INVALID_ENTITY_ID)
- `Any()` - Return true if at least one entity matches

Queries iterate `m_xEntityComponents` and use fold expressions to check for all required component types.

## Event System

Type-safe event dispatcher with immediate and deferred dispatch modes:

### Subscribing
```cpp
auto handle = Zenith_EventDispatcher::Get().Subscribe<EventType>(&CallbackFunction);
// Or with lambda:
auto handle = Zenith_EventDispatcher::Get().SubscribeLambda<EventType>(
    [](const EventType& event) { /* handle */ });
```

### Dispatching
```cpp
// Immediate dispatch
Zenith_EventDispatcher::Get().Dispatch(MyEvent{ data });

// Deferred (thread-safe, processed later on main thread)
Zenith_EventDispatcher::Get().QueueEvent(MyEvent{ data });
Zenith_EventDispatcher::Get().ProcessDeferredEvents(); // Call from main thread
```

### Built-in Events
- `Zenith_Event_EntityCreated` - Fired when entity is created
- `Zenith_Event_EntityDestroyed` - Fired when entity is destroyed
- `Zenith_Event_ComponentAdded` - Fired when component is added
- `Zenith_Event_ComponentRemoved` - Fired when component is removed

## Script Components

Custom behaviors use factory pattern for serialization. Behaviors must:
1. Inherit from `Zenith_ScriptBehaviour`
2. Use `ZENITH_BEHAVIOUR_TYPE_NAME(ClassName)` macro
3. Register via `RegisterBehaviour()` before scene load

Callbacks: `OnCreate()`, `OnUpdate(float)`, `OnCollisionEnter(entity)`, `OnCollisionExit(entityID)`

## Key Concepts

**Type Safety:** Template methods ensure compile-time type checking for component access.

**Serialization:** All components implement `WriteToDataStream()` and `ReadFromDataStream()` for scene save/load. The ComponentMeta system handles serialization order (lower order = serialized first) to respect dependencies.

**Thread Safety:**
- Scene modifications during rendering handled via deferred operations
- Event system provides thread-safe `QueueEvent()` for cross-thread event dispatch

**Entity Lifecycle:** Entities can have parent-child relationships via `m_uParentEntityID` member.

## Design Rationale

### Why Entity Is a Class, Not a POD Struct

The `Zenith_Entity` class may appear over-engineered at first glance since it's a "thin wrapper" that delegates most operations to the Scene. However, this design is intentional:

1. **Entity Creation Constructors Do Real Work**: The constructors allocate entity slots, register entities with the scene, set initial state, and auto-add TransformComponent. This is not trivial delegation.

2. **Validation and Lifecycle Dispatch**: Methods like `SetEnabled()` include validation (`IsValid()` checks) and dispatch lifecycle events (`OnEnable`/`OnDisable`) to all components. This encapsulates complex logic.

3. **Clean API Over Scattered Functions**: The class provides a clean object-oriented API (`entity.GetName()`, `entity.AddComponent<T>()`) vs verbose free functions (`GetEntityName(scene, id)`). This improves code readability throughout the codebase.

4. **Value Semantics with Handle Pattern**: The class is intentionally a lightweight handle (scene pointer + ID) that can be freely copied. The documentation explicitly states this is a "VALUE TYPE."

5. **Serialization Support**: The `WriteToDataStream`/`ReadFromDataStream` methods encapsulate entity serialization, keeping DataStream logic cohesive.

Converting to a POD struct would scatter entity logic across the codebase, lose the clean OOP API, and not significantly reduce complexity - just move it elsewhere.

### Why Event System Uses Virtual Inheritance

The `Zenith_EventSystem` callback hierarchy (`Zenith_CallbackBase` → `Zenith_CallbackWrapper<T>`) serves a legitimate purpose:

1. **Type Erasure for Deferred Queue**: The deferred event queue (`m_xDeferredEvents`) stores different event types. Type erasure requires polymorphism - either virtual dispatch or `std::function` (which uses virtual dispatch internally).

2. **Subscription Storage**: Subscriptions are stored by handle in an `unordered_map`. Different event types have different callback signatures, requiring type erasure to store them uniformly.

3. **No Performance Benefit from std::function**: Replacing with `std::function<void(const void*)>` would just substitute explicit virtual dispatch with implicit virtual dispatch. The overhead is equivalent.

4. **Current Design Is Type-Safe**: The callback wrappers know the exact event type at compile time, preserving type safety within the wrapper while providing runtime polymorphism for storage.
