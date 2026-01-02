# Components

This directory contains all component types for the Entity-Component System.

## Existing Components

| Component | Purpose |
|-----------|---------|
| `Zenith_TransformComponent` | Position, rotation, scale (added automatically to all entities) |
| `Zenith_CameraComponent` | View/projection matrices for rendering |
| `Zenith_ModelComponent` | Renderable 3D mesh with materials |
| `Zenith_ColliderComponent` | Physics collision shapes (Jolt integration) |
| `Zenith_TerrainComponent` | Heightmap-based terrain with streaming |
| `Zenith_ScriptComponent` | Custom behavior attachment via ScriptBehaviour |
| `Zenith_TextComponent` | 2D text rendering |
| `Zenith_UIComponent` | UI element support |

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

## Component Concept

Components must satisfy the `Zenith_Component` concept defined in `Zenith_Scene.h`:
- Constructible from `Zenith_Entity&`
- Destructible
- In ZENITH_TOOLS builds: must have `RenderPropertiesPanel()`

## Serialization Order

Component serialization order is determined by type name in `Zenith_ComponentMeta.cpp::GetSerializationOrder()`. Lower values serialize first. This matters for dependencies (e.g., TerrainComponent must serialize before ColliderComponent that depends on terrain data).

Current order:
1. Transform (0)
2. Model (10)
3. Camera (20)
4. Text (30)
5. Terrain (40)
6. Collider (50)
7. Script (60)
8. UI (70)

New components default to order 1000 (serialized last).

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

## Querying Multiple Components

Use the Query system to iterate entities with specific component combinations:

```cpp
scene.Query<TransformComponent, ColliderComponent>()
    .ForEach([](Zenith_EntityID id, TransformComponent& t, ColliderComponent& c) {
        // Process entities with both components
    });
```
