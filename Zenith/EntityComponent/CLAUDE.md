# Entity-Component System (ECS) Architecture

## Overview

Zenith uses a custom Entity-Component System (ECS) designed for:
- **Data-oriented design:** Components stored in contiguous arrays
- **Cache-friendly iteration:** Minimize cache misses during updates
- **Type-safe access:** Compile-time type checking
- **Flexible composition:** Entities defined by component combinations

## Core Concepts

### Entities

**Definition:** An entity is a lightweight ID (unsigned integer) that identifies an object in the scene.

```cpp
using Zenith_EntityID = u_int;
```

**Key Points:**
- Entities don't store data directly
- Entities are handles to collections of components
- Entities can be created/destroyed dynamically
- Entities can have parent-child relationships (partial implementation)

### Components

**Definition:** Components are pure data structures (POD or near-POD).

**Examples:**
```cpp
class Zenith_TransformComponent {
    Zenith_Maths::Vector3 m_xPosition;
    Zenith_Maths::Quaternion m_xRotation;
    Zenith_Maths::Vector3 m_xScale;
};

class Zenith_CameraComponent {
    float m_fFOV;
    float m_fNear;
    float m_fFar;
    float m_fAspect;
};

class Zenith_ModelComponent {
    Flux_Model* m_pxModel;
    Flux_Material* m_pxMaterial;
};
```

**Design Principles:**
- **No logic:** Components hold data only
- **Composable:** Entities combine components
- **Serializable:** Can be saved/loaded
- **Reflectable:** Type information available at runtime

### Systems

**Definition:** Systems contain logic that operates on components.

**Examples:**
- `Flux_StaticMeshes::Render()` - Renders all ModelComponents
- `Zenith_Physics::Update()` - Updates all ColliderComponents
- `Zenith_Scene::Update()` - Updates all ScriptComponents

## Architecture

### Scene Structure

```cpp
class Zenith_Scene {
    // Component storage
    Zenith_Vector<Zenith_ComponentPoolBase*> m_xComponents;
    
    // Entity-component mapping
    Zenith_Vector<std::unordered_map<TypeID, u_int>> m_xEntityComponents;
    
    // Entity metadata
    std::unordered_map<Zenith_EntityID, Zenith_Entity> m_xEntityMap;
    
    // Main camera
    Zenith_Entity* m_pxMainCameraEntity;
    
    // Thread safety
    Zenith_Mutex m_xMutex;
};
```

### Type ID System

**Purpose:** Assign unique IDs to component types at runtime

**Implementation:**
```cpp
class TypeIDGenerator {
    template<typename T>
    static TypeID GetTypeID() {
        static TypeID ls_uRet = s_uCounter++;
        return ls_uRet;
    }
    
private:
    inline static TypeID s_uCounter = 0;
};
```

**How it works:**
1. First call for `GetTypeID<TransformComponent>()` ? ID = 0
2. First call for `GetTypeID<CameraComponent>()` ? ID = 1
3. First call for `GetTypeID<ModelComponent>()` ? ID = 2
4. Subsequent calls return same ID (static variable)

**Type Safety:**
- Each component type gets unique ID
- IDs assigned in order of first access
- Compile-time type checking

### Component Pools

**Purpose:** Store components of same type contiguously

**Structure:**
```cpp
class Zenith_ComponentPoolBase {
    virtual ~Zenith_ComponentPoolBase() = default;
};

template<typename T>
class Zenith_ComponentPool : public Zenith_ComponentPoolBase {
public:
    Zenith_Vector<T> m_xData;  // Contiguous array of components
};
```

**Storage Layout:**
```
Scene
  ?
m_xComponents (vector of pool pointers, indexed by TypeID)
  ?? [0] ? TransformComponent Pool
  ?         ?? Vector<TransformComponent>
  ?              ?? [0] Entity 1's transform
  ?              ?? [1] Entity 2's transform
  ?              ?? [2] Entity 5's transform
  ?
  ?? [1] ? CameraComponent Pool
  ?         ?? Vector<CameraComponent>
  ?              ?? [0] Entity 2's camera
  ?              ?? [1] Entity 7's camera
  ?
  ?? [2] ? ModelComponent Pool
            ?? Vector<ModelComponent>
                 ?? [0] Entity 1's model
                 ?? [1] Entity 3's model
                 ?? [2] Entity 5's model
```

**Advantages:**
- **Cache friendly:** Components of same type stored together
- **Fast iteration:** Linear memory access
- **Type safety:** Each pool stores single component type
- **Dynamic allocation:** Pools created on first use

### Entity-Component Mapping

**Purpose:** Map entities to their component indices

**Structure:**
```cpp
// Index = EntityID
// Value = Map of (ComponentTypeID ? ComponentIndex)
Zenith_Vector<std::unordered_map<TypeID, u_int>> m_xEntityComponents;
```

**Example:**
```
Entity 1 (ID=1):
  m_xEntityComponents[1] = {
    TypeID(Transform) ? 0,  // Index in TransformComponent pool
    TypeID(Model) ? 0       // Index in ModelComponent pool
  }

Entity 2 (ID=2):
  m_xEntityComponents[2] = {
    TypeID(Transform) ? 1,  // Index in TransformComponent pool
    TypeID(Camera) ? 0      // Index in CameraComponent pool
  }
```

**Lookup Process:**
1. Get entity's component map: `m_xEntityComponents[entityID]`
2. Get component type ID: `TypeIDGenerator::GetTypeID<T>()`
3. Look up component index: `map[typeID]`
4. Access component pool: `GetComponentPool<T>()`
5. Get component: `pool->m_xData.Get(index)`

## Component Lifecycle

### Creating Components

```cpp
// From entity
entity.AddComponent<TransformComponent>(position, rotation, scale);

// Internal implementation
template<typename T, typename... Args>
T& CreateComponent(Zenith_EntityID uID, Args&&... args) {
    // Get or create pool for this component type
    Zenith_ComponentPool<T>* pool = GetComponentPool<T>();
    
    // Add component to pool
    u_int index = pool->m_xData.GetSize();
    pool->m_xData.EmplaceBack(std::forward<Args>(args)...);
    
    // Update entity-component mapping
    const TypeID typeID = TypeIDGenerator::GetTypeID<T>();
    m_xEntityComponents.Get(uID)[typeID] = index;
    
    // Return reference to new component
    return pool->m_xData.Get(index);
}
```

### Accessing Components

```cpp
// Check if entity has component
if (entity.HasComponent<TransformComponent>()) {
    // Get component reference
    TransformComponent& transform = entity.GetComponent<TransformComponent>();
    
    // Modify component
    transform.m_xPosition += velocity * dt;
}

// Internal implementation
template<typename T>
T& GetComponentFromEntity(Zenith_EntityID uID) {
    const TypeID typeID = TypeIDGenerator::GetTypeID<T>();
    
    // Get entity's component map
    auto& componentMap = m_xEntityComponents.Get(uID);
    
    // Get component index
    const u_int index = componentMap.at(typeID);
    
    // Get component pool
    auto* pool = GetComponentPool<T>();
    
    // Return component
    return pool->m_xData.Get(index);
}
```

### Removing Components

```cpp
entity.RemoveComponent<TransformComponent>();

// Internal implementation
template<typename T>
void RemoveComponentFromEntity(Zenith_EntityID uID) {
    const TypeID typeID = TypeIDGenerator::GetTypeID<T>();
    
    // Get entity's component map
    auto& componentMap = m_xEntityComponents.Get(uID);
    
    // Get component index
    const u_int index = componentMap.at(typeID);
    
    // Remove from pool
    GetComponentPool<T>()->m_xData.Remove(index);
    
    // Remove from entity mapping
    componentMap.erase(typeID);
}
```

**Note:** Removing from middle of vector is O(n), but entities are rarely destroyed mid-frame.

## Entity Wrapper

### Zenith_Entity Class

```cpp
class Zenith_Entity {
public:
    Zenith_Entity(Zenith_Scene* scene, const std::string& name);
    
    template<typename T, typename... Args>
    T& AddComponent(Args&&... args) {
        Zenith_Assert(!HasComponent<T>(), "Already has this component");
        return m_pxParentScene->CreateComponent<T>(m_uEntityID, std::forward<Args>(args)..., *this);
    }
    
    template<typename T>
    bool HasComponent() const {
        return m_pxParentScene->EntityHasComponent<T>(m_uEntityID);
    }
    
    template<typename T>
    T& GetComponent() const {
        Zenith_Assert(HasComponent<T>(), "Doesn't have this component");
        return m_pxParentScene->GetComponentFromEntity<T>(m_uEntityID);
    }
    
    template<typename T>
    void RemoveComponent() {
        Zenith_Assert(HasComponent<T>(), "Doesn't have this component");
        m_pxParentScene->RemoveComponentFromEntity<T>(m_uEntityID);
    }
    
    Zenith_EntityID GetEntityID() { return m_uEntityID; }
    
private:
    Zenith_Scene* m_pxParentScene;
    Zenith_EntityID m_uEntityID;
    Zenith_EntityID m_uParentEntityID;  // Not fully implemented
    std::string m_strName;
};
```

**Design:**
- Lightweight wrapper (pointer + ID)
- All operations delegate to scene
- Can be copied (value semantics)
- Invalidated if entity destroyed

## Common Components

### Zenith_TransformComponent

**Purpose:** Position, rotation, and scale in 3D space

```cpp
class Zenith_TransformComponent {
public:
    Zenith_Maths::Vector3 m_xPosition;
    Zenith_Maths::Quaternion m_xRotation;
    Zenith_Maths::Vector3 m_xScale;
    
    // Compute transformation matrix
    Zenith_Maths::Matrix4 GetTransformMatrix() const;
    
    // Hierarchical transforms (partial implementation)
    Zenith_TransformComponent* m_pxParent;
    Zenith_Maths::Matrix4 GetWorldTransformMatrix() const;
};
```

### Zenith_CameraComponent

**Purpose:** View and projection matrices for rendering

```cpp
class Zenith_CameraComponent {
public:
    enum CameraType {
        CAMERA_TYPE_PERSPECTIVE,
        CAMERA_TYPE_ORTHOGRAPHIC
    };
    
    void InitialisePerspective(
        const Zenith_Maths::Vector3& position,
        float pitch, float yaw,
        float fov, float nearPlane, float farPlane,
        float aspectRatio
    );
    
    void BuildViewMatrix(Zenith_Maths::Matrix4& out) const;
    void BuildProjectionMatrix(Zenith_Maths::Matrix4& out) const;
    
    Zenith_Maths::Vector3 ScreenSpaceToWorldSpace(Zenith_Maths::Vector3 screenPos);
    
private:
    CameraType m_eType;
    Zenith_Maths::Vector3 m_xPosition;
    float m_fPitch, m_fYaw;
    float m_fFOV, m_fNear, m_fFar, m_fAspect;
    Zenith_Entity m_xParentEntity;
};
```

### Zenith_ModelComponent

**Purpose:** Renderable mesh and material

```cpp
class Zenith_ModelComponent {
public:
    Flux_Model* m_pxModel;        // Mesh data
    Flux_Material* m_pxMaterial;  // Shader and textures
    bool m_bCastShadows;
    
private:
    Zenith_Entity m_xParentEntity;
};
```

### Zenith_ScriptComponent

**Purpose:** Attach custom behavior to entities

```cpp
class Zenith_ScriptComponent {
public:
    void SetBehaviour(Zenith_ScriptBehaviour* pBehaviour);
    void Update(float dt);

    // Serialization support (December 2024)
    void WriteToDataStream(Zenith_DataStream& xStream) const;
    void ReadFromDataStream(Zenith_DataStream& xStream);

private:
    Zenith_ScriptBehaviour* m_pxScriptBehaviour;
    Zenith_Entity m_xParentEntity;
};
```

#### Behavior Registry System (December 2024)

ScriptComponents use a **factory pattern** to support serialization of polymorphic behaviors. The `Zenith_BehaviourRegistry` singleton maintains a mapping from behavior type names to factory functions.

**Registry Architecture:**
```cpp
class Zenith_BehaviourRegistry {
public:
    using BehaviourFactoryFunc = Zenith_ScriptBehaviour* (*)(Zenith_Entity&);

    static Zenith_BehaviourRegistry& Get() {
        static Zenith_BehaviourRegistry s_xInstance;
        return s_xInstance;
    }

    void RegisterBehaviour(const char* szTypeName, BehaviourFactoryFunc fnFactory);
    Zenith_ScriptBehaviour* CreateBehaviour(const char* szTypeName, Zenith_Entity& xEntity);

private:
    std::unordered_map<std::string, BehaviourFactoryFunc> m_xFactoryMap;
};
```

**The ZENITH_BEHAVIOUR_TYPE_NAME Macro:**

All behaviors that need serialization MUST include this macro:

```cpp
#define ZENITH_BEHAVIOUR_TYPE_NAME(TypeName) \
    virtual const char* GetBehaviourTypeName() const override { return #TypeName; } \
    static Zenith_ScriptBehaviour* CreateInstance(Zenith_Entity& xEntity) { return new TypeName(xEntity); } \
    static void RegisterBehaviour() { Zenith_BehaviourRegistry::Get().RegisterBehaviour(#TypeName, &TypeName::CreateInstance); }
```

**What this provides:**
1. `GetBehaviourTypeName()` - Returns the class name as a string (for serialization)
2. `CreateInstance()` - Static factory function to create instances
3. `RegisterBehaviour()` - Static function to register with the global registry

**User-defined Behavior Example:**
```cpp
class PlayerController_Behaviour ZENITH_FINAL : public Zenith_ScriptBehaviour {
public:
    ZENITH_BEHAVIOUR_TYPE_NAME(PlayerController_Behaviour)

    PlayerController_Behaviour(Zenith_Entity& xEntity) : Zenith_ScriptBehaviour(xEntity) {}

    void OnCreate() override {
        // Initialization - called after construction
    }

    void OnUpdate(float dt) override {
        // Per-frame logic
        if (Zenith_Input::IsKeyPressed(KEY_W)) {
            auto& transform = m_xParentEntity.GetComponent<Zenith_TransformComponent>();
            transform.m_xPosition.z += 5.0f * dt;
        }
    }
};
```

**Registration (REQUIRED for serialization):**
Behaviors MUST be registered before loading scenes that reference them:

```cpp
void OnGameStart() {
    // Register all behaviors at startup
    PlayerController_Behaviour::RegisterBehaviour();
    EnemyAI_Behaviour::RegisterBehaviour();
    // etc.
}
```

**Serialization Flow:**
```
Save:
1. WriteToDataStream() called on ScriptComponent
2. Writes bool (has behavior?)
3. If true, writes behavior type name string

Load:
1. ReadFromDataStream() called on ScriptComponent
2. Reads bool (has behavior?)
3. If true, reads type name string
4. Looks up factory function in registry
5. Calls factory to create new behavior instance
6. Calls OnCreate() on the new behavior
```

**CRITICAL: Why This Was Needed (December 2024 Bug Fix)**

Without the behavior registry, ScriptComponents were NOT serialized. This caused:
- **First Play:** Works - behaviors attached at runtime
- **Stop then Second Play:** Fails - scene restored from backup, but ScriptComponent deserialization couldn't recreate polymorphic behaviors

The fix required:
1. Adding the behavior registry factory pattern
2. Adding the `ZENITH_BEHAVIOUR_TYPE_NAME` macro
3. Implementing `WriteToDataStream`/`ReadFromDataStream` in ScriptComponent
4. Adding ScriptComponent to entity/scene serialization lists
5. Registering all game behaviors at startup

### Zenith_ColliderComponent

Manages physics collision shapes.

```cpp
class Zenith_ColliderComponent {
public:
    // Jolt Physics handles
    JPH::Body* m_pxBody;
    
    // Collision shape types
    enum ColliderType {
        BOX,
        SPHERE,
        CAPSULE,
        MESH
    };
    
    ColliderType m_eType;
    
    // Dimensions (type-dependent)
    Zenith_Maths::Vector3 m_xHalfExtents;  // Box
    float m_fRadius;                        // Sphere/Capsule
    float m_fHeight;                        // Capsule
    
    // Physics properties
    float m_fMass;
    float m_fFriction;
    float m_fRestitution;
    bool m_bIsKinematic;
};
```

**Thread Safety Considerations:**

When synchronizing physics and rendering:

```cpp
void SyncPhysicsToTransform() {
    // Read from Jolt Physics body (on physics thread)
    if (m_pxBody && m_pxBody->IsActive()) {
        const JPH::RVec3& physicsPosition = m_pxBody->GetPosition();
        const JPH::Quat& physicsRotation = m_pxBody->GetRotation();
        
        // Write to transform component (requires lock if accessed by render thread)
        Zenith_TransformComponent& xTransform = GetEntity().GetComponent<Zenith_TransformComponent>();
        xTransform.SetPosition(Zenith_Maths::Vector3(physicsPosition.GetX(), physicsPosition.GetY(), physicsPosition.GetZ()));
        xTransform.SetRotation(Zenith_Maths::Quaternion(physicsRotation.GetX(), physicsRotation.GetY(), physicsRotation.GetZ(), physicsRotation.GetW()));
    }
}
```

### Zenith_TextComponent

**Purpose:** Render 2D text

```cpp
class Zenith_TextComponent {
public:
    std::string m_strText;
    Zenith_Maths::Vector2 m_xPosition;  // Screen space
    float m_fScale;
    Zenith_Maths::Vector4 m_xColor;
    
private:
    Zenith_Entity m_xParentEntity;
};
```

### Zenith_TerrainComponent

**Purpose:** Heightmap-based terrain

```cpp
class Zenith_TerrainComponent {
public:
    Flux_Terrain* m_pxTerrain;  // Heightmap data
    
private:
    Zenith_Entity m_xParentEntity;
};
```

## System Updates

### Scene Update

```cpp
// Called every frame from state machine
void Zenith_Scene::Update(float dt) {
    // Update all script components
    UpdateScripts(dt);
}

static void UpdateScripts(float dt) {
    // Get all script components
    Zenith_Vector<Zenith_ScriptComponent*> scripts;
    s_xCurrentScene.GetAllOfComponentType<Zenith_ScriptComponent>(scripts);
    
    // Update each script
    for (Zenith_ScriptComponent* script : scripts) {
        script->Update(dt);
    }
}
```

**Multithreaded Version (Potential Optimization):**
```cpp
static void UpdateScriptTask(void* data, u_int index, u_int count) {
    Zenith_Vector<Zenith_ScriptComponent*>* scripts = 
        static_cast<Zenith_Vector<Zenith_ScriptComponent*>*>(data);
    
    scripts->Get(index)->Update(Zenith_Core::GetDt());
}

static void UpdateScripts(float dt) {
    Zenith_Vector<Zenith_ScriptComponent*> scripts;
    s_xCurrentScene.GetAllOfComponentType<Zenith_ScriptComponent>(scripts);
    
    if (scripts.GetSize() == 0)
        return;
    
    // Parallel update
    Zenith_TaskArray updateTask(
        ZENITH_PROFILE_INDEX__ECS_UPDATE_SCRIPTS,
        UpdateScriptTask,
        &scripts,
        scripts.GetSize(),
        true  // Submitting thread joins
    );
    
    Zenith_TaskSystem::SubmitTaskArray(&updateTask);
    updateTask.WaitUntilComplete();
}
```

### Rendering System Integration

```cpp
void Flux_StaticMeshes::Render() {
    // Get all entities with ModelComponent
    Zenith_Vector<Zenith_ModelComponent*> models;
    Zenith_Scene::GetCurrentScene().GetAllOfComponentType<Zenith_ModelComponent>(models);
    
    // Create command list
    Flux_CommandList* cmdList = GetCommandList();
    cmdList->Reset(true);
    
    cmdList->AddCommand<Flux_CommandSetPipeline>(s_pxPipeline);
    
    // Render each model
    for (Zenith_ModelComponent* model : models) {
        // Get transform
        Zenith_Entity& entity = model->m_xParentEntity;
        Zenith_TransformComponent& transform = entity.GetComponent<TransformComponent>();
        
        // Push transform matrix
        Zenith_Maths::Matrix4 modelMatrix = transform.GetWorldTransformMatrix();
        cmdList->AddCommand<Flux_CommandPushConstant>(&modelMatrix, sizeof(modelMatrix));
        
        // Bind mesh
        cmdList->AddCommand<Flux_CommandSetVertexBuffer>(model->m_pxModel->GetVertexBuffer());
        cmdList->AddCommand<Flux_CommandSetIndexBuffer>(model->m_pxModel->GetIndexBuffer());
        
        // Bind material
        cmdList->AddCommand<Flux_CommandBeginBind>(0);
        cmdList->AddCommand<Flux_CommandBindSRV>(&model->m_pxMaterial->GetAlbedoSRV(), 0);
        cmdList->AddCommand<Flux_CommandBindSRV>(&model->m_pxMaterial->GetNormalSRV(), 1);
        
        // Draw
        cmdList->AddCommand<Flux_CommandDrawIndexed>(model->m_pxModel->GetIndexCount());
    }
    
    // Submit
    Flux::SubmitCommandList(cmdList, s_xTargetSetup, RENDER_ORDER_GBUFFER);
}
```

## Serialization

### Entity Serialization

```cpp
void Zenith_Entity::Serialize(std::ofstream& out) {
    // Write entity ID
    out << m_uEntityID << "\n";
    
    // Write name
    out << m_strName << "\n";
    
    // Write parent ID
    out << m_uParentEntityID << "\n";
    
    // Write component count
    auto& componentMap = m_pxParentScene->m_xEntityComponents.Get(m_uEntityID);
    out << componentMap.size() << "\n";
    
    // Write each component
    for (auto& [typeID, index] : componentMap) {
        // Write type ID
        out << typeID << "\n";
        
        // Serialize component data
        // (Type-specific serialization not shown)
    }
}
```

### Scene Serialization

```cpp
void Zenith_Scene::Serialize(const std::string& filename) {
    std::ofstream out(filename);
    
    // Write entity count
    out << m_xEntityMap.size() << "\n";
    
    // Serialize each entity
    for (auto& [id, entity] : m_xEntityMap) {
        entity.Serialize(out);
    }
    
    out.close();
}
```

## Performance Characteristics

### Memory Layout

**Best Case (All entities have same components):**
```
TransformComponent Pool: [T1][T2][T3][T4]...  // Contiguous
ModelComponent Pool:      [M1][M2][M3][M4]...  // Contiguous
```

**Iteration Performance:**
- Linear memory access
- Excellent cache locality
- ~100 cycles per entity (fast)

**Worst Case (Entities have different components):**
```
TransformComponent Pool: [T1][T2].....[T50]  // Some entities missing
ModelComponent Pool:      [M1]........[M45]  // Different entities
```

**Iteration Performance:**
- Still linear memory access
- Some wasted space
- ~100 cycles per entity (still fast)

### Component Access

**Get Component:**
- Hash map lookup: ~50 cycles
- Vector access: ~10 cycles
- **Total: ~60 cycles**

**Has Component:**
- Hash map lookup: ~50 cycles
- **Total: ~50 cycles**

**Add Component:**
- Hash map insert: ~100 cycles
- Vector push: ~20 cycles
- **Total: ~120 cycles**

### Scaling

**Number of Entities:**
- 10 entities: Excellent performance
- 100 entities: Excellent performance
- 1,000 entities: Good performance
- 10,000 entities: Moderate performance (hash map lookups add up)
- 100,000+ entities: Consider pure ECS (archetype-based)

**Number of Component Types:**
- Unlimited (type IDs generated dynamically)
- Each type has dedicated pool (no interference)

## Best Practices

### Component Design

**Good:**
```cpp
class Zenith_HealthComponent {
    float m_fCurrentHealth;
    float m_fMaxHealth;
};
```

**Bad:**
```cpp
class Zenith_HealthComponent {
    float m_fCurrentHealth;
    float m_fMaxHealth;
    
    void TakeDamage(float amount);  // Logic in component (bad)
    void Heal(float amount);        // Logic in component (bad)
};
```

**Better:**
```cpp
// Component (data only)
class Zenith_HealthComponent {
    float m_fCurrentHealth;
    float m_fMaxHealth;
};

// System (logic)
class Zenith_HealthSystem {
    static void TakeDamage(Zenith_Entity& entity, float amount) {
        auto& health = entity.GetComponent<HealthComponent>();
        health.m_fCurrentHealth -= amount;
        if (health.m_fCurrentHealth <= 0) {
            // Trigger death
        }
    }
};
```

### Entity Usage

**Efficient:**
```cpp
// Cache component references
Zenith_Entity& player = GetPlayerEntity();
TransformComponent& transform = player.GetComponent<TransformComponent>();

// Update multiple times
for (int i = 0; i < 100; i++) {
    transform.m_xPosition.x += 0.01f;
}
```

**Inefficient:**
```cpp
// Re-lookup every time
Zenith_Entity& player = GetPlayerEntity();

for (int i = 0; i < 100; i++) {
    player.GetComponent<TransformComponent>().m_xPosition.x += 0.01f;
}
```

### Iteration Patterns

**Good:**
```cpp
// Get all components once
Zenith_Vector<ModelComponent*> models;
scene.GetAllOfComponentType<ModelComponent>(models);

// Iterate (cache-friendly)
for (ModelComponent* model : models) {
    // Process model
}
```

**Bad:**
```cpp
// Iterate entities, check components
for (auto& [id, entity] : scene.m_xEntityMap) {
    if (entity.HasComponent<ModelComponent>()) {
        auto& model = entity.GetComponent<ModelComponent>();
        // Process model
    }
}
```

## Limitations & Future Work

### Current Limitations

1. **No Archetype System:** Entities with different component combinations stored separately
2. **Sparse Component Storage:** Gaps in component pools when entities destroyed
3. **Parent-Child Hierarchies:** Incomplete implementation
4. **No Component Dependencies:** Can't enforce "RequiresComponent" relationships
5. **Linear Remove:** Removing component from middle is O(n)

### Future Improvements

1. **Archetype-Based Storage:**
   - Group entities by component combination
   - Eliminate hash map lookups
   - Better cache locality

2. **Stable Indices:**
   - Use free list for component indices
   - Avoid gaps in pools
   - Faster iteration

3. **Hierarchical Transforms:**
   - Complete parent-child implementation
   - Automatic matrix propagation

4. **Component Relationships:**
   - Compile-time dependencies
   - Automatic component creation

5. **Multithreaded Updates:**
   - Parallel component iteration
   - Read/write dependency tracking

## Thread Safety and Concurrency

### Scene Mutation During Rendering

**CRITICAL ISSUE:** Scene data (component pools, entity maps) is accessed by render tasks running on worker threads. Any modification to the scene while these tasks are active causes crashes due to concurrent access.

**Examples of Unsafe Operations:**
- `Zenith_Scene::Reset()` - Destroys all component pools
- `Zenith_Scene::LoadFromFile()` - Calls Reset() then reconstructs scene
- Adding/removing entities during rendering
- Resizing component pools during rendering

### Safe Scene Loading and Saving Architecture

**Problem:** Prior to the fix, "Open Scene" and "Save Scene" in the editor were triggered from the ImGui menu, which is rendered during `SubmitRenderTasks()`. This meant scene operations were called while render tasks were active:

**Unsafe Call Stack (OLD):**
```
Zenith_Core::Zenith_MainLoop()
  └─ SubmitRenderTasks()
      └─ RenderImGui()
          └─ Zenith_Editor::Render()
              └─ RenderMainMenuBar()
                  └─ ImGui::MenuItem("Open Scene") clicked
                      └─ Zenith_Scene::LoadFromFile()  // CRASH: render tasks still active!
                          └─ Reset()  // Destroys pools being read by render tasks
```

**Solution:** Deferred scene loading and saving via `Zenith_Editor::Update()`

**Safe Call Stack (NEW):**
```
Frame N:
  Zenith_Core::Zenith_MainLoop()
    └─ SubmitRenderTasks()
        └─ RenderImGui()
            └─ Zenith_Editor::Render()
                └─ RenderMainMenuBar()
                    └─ ImGui::MenuItem("Open Scene") clicked
                        └─ Sets s_bPendingSceneLoad = true  // Just set flag, don't load!
    └─ WaitForRenderTasks()  // All render tasks complete

Frame N+1:
  Zenith_Core::Zenith_MainLoop()
    └─ Zenith_Editor::Update()  // Called BEFORE any rendering
        └─ if (s_bPendingSceneLoad)
            └─ Zenith_Scene::LoadFromFile()  // SAFE: no tasks active
                └─ Reset()  // Safe to destroy pools
```

**Implementation Details:**

```cpp
// Zenith_Editor.h
class Zenith_Editor {
private:
    // Deferred scene operations (to avoid concurrent access during render tasks)
    static bool s_bPendingSceneLoad;
    static std::string s_strPendingSceneLoadPath;
    static bool s_bPendingSceneSave;
    static std::string s_strPendingSceneSavePath;
};

// Zenith_Editor.cpp - Menu handlers
if (ImGui::MenuItem("Open Scene", "Ctrl+O")) {
    // Do NOT load immediately - just queue it
    s_strPendingSceneLoadPath = "scene.zscen";
    s_bPendingSceneLoad = true;
}

if (ImGui::MenuItem("Save Scene", "Ctrl+S")) {
    // Do NOT save immediately - just queue it
    s_strPendingSceneSavePath = "scene.zscen";
    s_bPendingSceneSave = true;
}

// Zenith_Editor.cpp - Update function
void Zenith_Editor::Update() {
    // Process deferred saves FIRST
    if (s_bPendingSceneSave) {
        s_bPendingSceneSave = false;
        // Safe to save - no render tasks active
        Zenith_Scene::GetCurrentScene().SaveToFile(s_strPendingSceneSavePath);
        s_strPendingSceneSavePath.clear();
    }

    // Process deferred loads (after saves, so save-then-load works correctly)
    if (s_bPendingSceneLoad) {
        s_bPendingSceneLoad = false;
        // Safe to load - no render tasks active
        Zenith_Scene::GetCurrentScene().LoadFromFile(s_strPendingSceneLoadPath);
        ClearSelection();  // Entity pointers now invalid
        s_strPendingSceneLoadPath.clear();
    }
    // ... rest of update
}

// Zenith_Core.cpp - Main loop
void Zenith_Core::Zenith_MainLoop() {
    // ... begin frame setup ...

#ifdef ZENITH_TOOLS
    Zenith_Editor::Update();  // BEFORE any rendering!
#endif

    // Physics, scene update...
    SubmitRenderTasks();  // Now safe - scene won't change during render
    WaitForRenderTasks();
    // ... end frame ...
}
```

**Key Guarantees:**
1. `Zenith_Editor::Update()` is called **before** `SubmitRenderTasks()`
2. Scene loading and saving only happen in `Update()`, never during rendering
3. Saves processed before loads (ensuring save-then-load workflows work correctly)
4. One frame delay between user action and scene operation (acceptable)
5. Entity pointers cleared after load to prevent dangling references

### Entity Component Array Pre-Allocation

**Context:** When loading a scene from file, entities can have arbitrary IDs (e.g., Entity ID 100, 200, 500) due to how they were created/deleted during editing.

**Data Structure:**
```cpp
class Zenith_Scene {
    // Indexed by EntityID
    Zenith_Vector<std::unordered_map<TypeID, u_int>> m_xEntityComponents;
};
```

**Problem:** If we're loading Entity ID 100, but `m_xEntityComponents` only has 10 elements, accessing `m_xEntityComponents[100]` is out of bounds.

**Solution in LoadFromFile():**
```cpp
void Zenith_Scene::LoadFromFile(const std::string& strFilename) {
    for (u_int u = 0; u < uNumEntities; u++) {
        Zenith_EntityID uEntityID;
        xStream >> uEntityID;

        // CRITICAL: Ensure vector is large enough for this entity ID
        // Without this, accessing m_xEntityComponents[uEntityID] would crash
        while (m_xEntityComponents.GetSize() <= uEntityID) {
            m_xEntityComponents.PushBack({});  // Add empty component map
        }

        // Now safe to create entity with this ID
        Zenith_Entity xEntity(this, uEntityID, ...);
    }
}
```

**Why This Works:**
1. `m_xEntityComponents` is indexed directly by entity ID
2. Pre-allocating ensures `m_xEntityComponents[uEntityID]` is always valid
3. Empty maps for "gaps" (unused entity IDs) have minimal memory cost
4. When components are added, they're inserted into the pre-allocated map

**Example:**
```
Loading entities with IDs: 5, 10, 15

Before pre-allocation:
  m_xEntityComponents.size() = 0

After loading Entity 5:
  m_xEntityComponents = [{}, {}, {}, {}, {}, {}]
                         0   1   2   3   4   5

After loading Entity 10:
  m_xEntityComponents = [{}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}]
                         0   1   2   3   4   5   6   7   8   9  10

After loading Entity 15:
  m_xEntityComponents = [{}, {}, {}, ..., {}, {}, {}]
                         0   1   2  ...  13  14  15
```

Entities 0-4, 6-9, 11-14 have empty component maps (no memory overhead beyond the map itself), while entities 5, 10, 15 have their actual component mappings.

### Component Deserialization Patterns

**Important:** Components must support deserialization-friendly constructors for scene loading to work correctly.

#### Pattern 1: Simple Components (Default Constructor)
Most components can use a simple default constructor or minimal constructor:

```cpp
class Zenith_TransformComponent {
public:
    Zenith_TransformComponent(Zenith_Entity& xEntity)
        : m_xParentEntity(xEntity) {}

    void ReadFromDataStream(Zenith_DataStream& xStream);
    void WriteToDataStream(Zenith_DataStream& xStream) const;
};
```

**Deserialization:**
```cpp
auto& component = entity.AddComponent<Zenith_TransformComponent>();
component.ReadFromDataStream(xStream);  // Populates all data
```

#### Pattern 2: Asset-Dependent Components (TerrainComponent)

Components that reference assets (meshes, materials, textures) must be loadable even when assets might not exist initially.

**Problem:** `Zenith_TerrainComponent` originally only had a constructor requiring all assets:
```cpp
// ORIGINAL - Doesn't work for deserialization!
Zenith_TerrainComponent(
    Flux_MeshGeometry& xRenderGeometry,
    Flux_MeshGeometry& xPhysicsGeometry,
    Flux_MeshGeometry& xWaterGeometry,
    Flux_Material& xMaterial0,
    Flux_Material& xMaterial1,
    Zenith_Maths::Vector2 xPosition_2D,
    Zenith_Entity& xEntity
);
```

**Solution:** Add a minimal constructor for deserialization:
```cpp
// Minimal constructor for deserialization
Zenith_TerrainComponent(Zenith_Entity& xEntity)
    : m_xParentEntity(xEntity)
    , m_pxRenderGeometry(nullptr)
    , m_pxPhysicsGeometry(nullptr)
    , m_pxWaterGeometry(nullptr)
    , m_pxMaterial0(nullptr)
    , m_pxMaterial1(nullptr)
    , m_xPosition_2D(FLT_MAX, FLT_MAX)
{
};

// Full constructor for runtime creation (unchanged)
Zenith_TerrainComponent(
    Flux_MeshGeometry& xRenderGeometry,
    Flux_MeshGeometry& xPhysicsGeometry,
    Flux_MeshGeometry& xWaterGeometry,
    Flux_Material& xMaterial0,
    Flux_Material& xMaterial1,
    Zenith_Maths::Vector2 xPosition_2D,
    Zenith_Entity& xEntity
);
```

**How ReadFromDataStream Handles Assets:**

```cpp
void Zenith_TerrainComponent::ReadFromDataStream(Zenith_DataStream& xStream)
{
    // Read asset names from stream
    std::string strRenderGeometryName;
    std::string strPhysicsGeometryName;
    std::string strWaterGeometryName;
    std::string strMaterial0Name;
    std::string strMaterial1Name;

    xStream >> strRenderGeometryName;
    xStream >> strPhysicsGeometryName;
    xStream >> strWaterGeometryName;
    xStream >> strMaterial0Name;
    xStream >> strMaterial1Name;

    // Look up assets using AssetHandler
    // Assets MUST be loaded before deserializing the scene!
    if (Zenith_AssetHandler::MeshExists(strRenderGeometryName) &&
        Zenith_AssetHandler::MeshExists(strPhysicsGeometryName) &&
        Zenith_AssetHandler::MeshExists(strWaterGeometryName) &&
        Zenith_AssetHandler::MaterialExists(strMaterial0Name) &&
        Zenith_AssetHandler::MaterialExists(strMaterial1Name))
    {
        m_pxRenderGeometry = &Zenith_AssetHandler::GetMesh(strRenderGeometryName);
        m_pxPhysicsGeometry = &Zenith_AssetHandler::GetMesh(strPhysicsGeometryName);
        m_pxWaterGeometry = &Zenith_AssetHandler::GetMesh(strWaterGeometryName);
        m_pxMaterial0 = &Zenith_AssetHandler::GetMaterial(strMaterial0Name);
        m_pxMaterial1 = &Zenith_AssetHandler::GetMaterial(strMaterial1Name);
    }
    else
    {
        Zenith_Assert(false, "Referenced assets not found during TerrainComponent deserialization");
    }

    // Read remaining component data
    xStream >> m_xPosition_2D;
}
```

**Key Points:**
1. **Asset Names, Not Pointers:** Serialization saves asset names as strings, not pointers
2. **Asset Lookup:** `Zenith_AssetHandler` provides a global registry to look up assets by name
3. **Load Order Critical:** Assets MUST be loaded before calling `LoadFromFile()` on the scene
4. **Graceful Failure:** If assets don't exist, assertion fails (could be changed to warning for resilience)

**Usage in Scene Loading:**
```cpp
// In Zenith_Scene::LoadFromFile()
if (strComponentType == "TerrainComponent")
{
    // Create component with minimal constructor
    Zenith_TerrainComponent& xComponent =
        xEntityInMap.AddComponent<Zenith_TerrainComponent>();

    // ReadFromDataStream handles asset lookup and population
    xComponent.ReadFromDataStream(xStream);
}
```

#### Pattern 3: AddComponent Forwarding Mechanism

**Important Mechanism:** `Entity::AddComponent()` automatically appends the entity reference:

```cpp
template<typename T, typename... Args>
T& AddComponent(Args&&... args) {
    return m_pxParentScene->CreateComponent<T>(
        m_uEntityID,
        std::forward<Args>(args)...,
        *this  // ← Entity reference automatically added!
    );
}
```

This means:
- `entity.AddComponent<TransformComponent>()` → calls constructor with `(entity)`
- `entity.AddComponent<TerrainComponent>()` → calls constructor with `(entity)`
- `entity.AddComponent<CustomComponent>(param1, param2)` → calls constructor with `(param1, param2, entity)`

All components receive the entity reference as their **final constructor parameter**.

#### Pattern 4: Component Dependency Ordering

**CRITICAL:** Components with dependencies on other components must be serialized in the correct order!

**Problem:** `Zenith_ColliderComponent` can have terrain colliders, which require `Zenith_TerrainComponent` to exist:

```cpp
// In Zenith_ColliderComponent::AddCollider()
case COLLISION_VOLUME_TYPE_TERRAIN:
{
    // ❌ Crashes if TerrainComponent doesn't exist yet!
    Zenith_Assert(m_xParentEntity.HasComponent<Zenith_TerrainComponent>(),
                  "Can't have a terrain collider without a terrain component");
    const Zenith_TerrainComponent& xTerrain = m_xParentEntity.GetComponent<Zenith_TerrainComponent>();
    // Uses terrain mesh data...
}
```

**If serialization order is wrong:**
```
Save order: Transform → Model → Camera → Collider → Text → Terrain
                                          ↑                    ↑
                                      Saved 4th           Saved 6th

Load order: Same order as saved
  1. Transform ✅
  2. Model ✅
  3. Camera ✅
  4. Collider → ReadFromDataStream() → AddCollider(TERRAIN)
              → Checks for TerrainComponent... NOT LOADED YET!
              → ❌ CRASH: "Can't have a terrain collider without a terrain component"
  5. Text
  6. Terrain (too late!)
```

**Solution:** TerrainComponent MUST be serialized BEFORE ColliderComponent:

```cpp
// In Zenith_Entity::WriteToDataStream()
// IMPORTANT: Order matters for deserialization!
// Components with dependencies must be serialized AFTER their dependencies
if (HasComponent<Zenith_TransformComponent>())
    xComponentTypes.push_back("TransformComponent");
if (HasComponent<Zenith_ModelComponent>())
    xComponentTypes.push_back("ModelComponent");
if (HasComponent<Zenith_CameraComponent>())
    xComponentTypes.push_back("CameraComponent");
if (HasComponent<Zenith_TextComponent>())
    xComponentTypes.push_back("TextComponent");
// TerrainComponent BEFORE ColliderComponent!
if (HasComponent<Zenith_TerrainComponent>())
    xComponentTypes.push_back("TerrainComponent");
if (HasComponent<Zenith_ColliderComponent>())
    xComponentTypes.push_back("ColliderComponent");
// ScriptComponent LAST - behaviors may depend on other components
if (HasComponent<Zenith_ScriptComponent>())
    xComponentTypes.push_back("ScriptComponent");
```

**Correct load order:**
```
Load order: Transform → Model → Camera → Text → Terrain → Collider → Script
                                                  ↑         ↑          ↑
                                              Loaded 5th  Loaded 6th  Loaded 7th

  1. Transform ✅
  2. Model ✅
  3. Camera ✅
  4. Text ✅
  5. Terrain ✅
  6. Collider → ReadFromDataStream() → AddCollider(TERRAIN)
              → Checks for TerrainComponent... ✅ EXISTS!
              → Uses terrain mesh data
              → ✅ SUCCESS!
  7. Script → ReadFromDataStream() → BehaviourRegistry::CreateBehaviour()
            → Behavior's OnCreate() may access other components
            → All components exist ✅ SUCCESS!
```

**General Rule:** When adding new components:
1. **Identify dependencies:** Does the component's constructor or `ReadFromDataStream` access other components?
2. **Update serialization order:** Ensure dependencies are serialized BEFORE dependent components
3. **Test loading:** Verify scene loads without assertions

**Common Dependency Chains:**
- `ColliderComponent` → depends on → `TerrainComponent` (for terrain colliders)
- `ScriptComponent` → may depend on → any component (behaviors access components in `OnCreate()`)
- `ChildComponent` → would depend on → `ParentComponent` (hypothetical example)

**ScriptComponent Notes (December 2024):**
ScriptComponent serialization was added to fix play/stop restore issues in the editor.
It uses a behavior registry factory pattern - see [ScriptComponent section](#zenith_scriptcomponent) above for details.

## Asset Lifetime Management and Scene Loading

### The Problem: Component-Owned Assets

Some components create and "own" assets through methods like `ModelComponent::LoadMeshesFromDir()`. These components track created assets in arrays (`m_xCreatedMeshes`, `m_xCreatedMaterials`, `m_xCreatedTextures`) and delete them in the destructor to prevent leaks.

**The Issue During Scene Loading:**

When you save and load a scene:

```cpp
// Runtime: Create components with LoadMeshesFromDir
Zenith_ModelComponent& xModel = entity.AddComponent<Zenith_ModelComponent>();
xModel.LoadMeshesFromDir("Meshes/ogre");  // Creates assets: "ogre01", "ogre_mesh_0", etc.
                                           // Tracks them in m_xCreatedMaterials, m_xCreatedMeshes

// Save scene: Serializes asset names
scene.SaveToFile("scene.zscen");  // Saves: "ogre01", "ogre_mesh_0"

// Load scene: CRASHES!
scene.LoadFromFile("scene.zscen");
  └─ Reset()  // Destroys all components
      └─ ~ModelComponent()  // Deletes assets from AssetHandler
          └─ DeleteMaterial("ogre01")  // ❌ Asset deleted!
          └─ DeleteMesh("ogre_mesh_0")  // ❌ Asset deleted!
  └─ ReadFromDataStream()  // Tries to find "ogre01", "ogre_mesh_0"
      └─ MaterialExists("ogre01")  // ❌ Returns false!
      └─ Zenith_Assert(false, "Referenced assets not found during ModelComponent deserialization")
```

**Root Cause:** `LoadFromFile()` calls `Reset()` which destroys old components, deleting their assets. Then deserialization tries to reference those same assets, but they're gone!

### The Solution: IsLoadingScene Flag

**Implementation:**

```cpp
// Zenith_Scene.h
class Zenith_Scene {
public:
    static bool IsLoadingScene() { return s_bIsLoadingScene; }
private:
    static bool s_bIsLoadingScene;
};

// Zenith_Scene.cpp
void Zenith_Scene::LoadFromFile(const std::string& strFilename)
{
    // Set flag BEFORE Reset()
    s_bIsLoadingScene = true;

    Reset();  // Destroys components, but they check the flag

    // ... deserialize scene ...

    // Clear flag after loading complete
    s_bIsLoadingScene = false;
}

// Zenith_ModelComponent.cpp
bool Zenith_ModelComponent_ShouldDeleteAssets()
{
    return !Zenith_Scene::IsLoadingScene();
}

// Zenith_ModelComponent.h
~Zenith_ModelComponent()
{
    extern bool Zenith_ModelComponent_ShouldDeleteAssets();

    if (Zenith_ModelComponent_ShouldDeleteAssets())
    {
        // Only delete assets if NOT loading a scene
        for (uint32_t u = 0; u < m_xCreatedMaterials.GetSize(); u++)
        {
            Zenith_AssetHandler::DeleteMaterial(m_xCreatedMaterials.Get(u));
        }
        // ... delete meshes, textures ...
    }
}
```

**How It Works:**

1. **Before Reset():** `LoadFromFile` sets `s_bIsLoadingScene = true`
2. **During Reset():** Component destructors check `IsLoadingScene()`
   - If true: Skip asset deletion (assets preserved for deserialization)
   - If false: Delete assets normally (component being individually destroyed)
3. **After Deserialization:** `LoadFromFile` sets `s_bIsLoadingScene = false`

**Result:** Assets created by `LoadMeshesFromDir` persist through scene loads!

### Why Forward Declaration is Needed

In `Zenith_ModelComponent.h`, we use a forward declaration instead of including `Zenith_Scene.h`:

```cpp
extern bool Zenith_ModelComponent_ShouldDeleteAssets();
```

**Reason:** Circular dependency prevention:
- `Zenith_Scene.h` includes `Zenith_ModelComponent.h` (for deserialization)
- If `Zenith_ModelComponent.h` included `Zenith_Scene.h`, we'd have a circular include

The implementation in `Zenith_ModelComponent.cpp` can safely include `Zenith_Scene.h` since `.cpp` files don't cause circular dependencies.

### Asset Naming and LoadMeshesFromDir

**Important Behavior:** `LoadMeshesFromDir` uses a static counter to create unique material names:

```cpp
void LoadMeshesFromDir(const std::filesystem::path& strPath, ...)
{
    static u_int ls_uCount = 0;
    ls_uCount++;  // Increments EVERY call

    // Material name includes the counter
    const std::string strMatName = strLeaf + std::to_string(uMatIndex) + std::to_string(ls_uCount);
    //                               ↑                                     ↑
    //                          directory name                       unique counter
}
```

**Why This Matters:**

- **First call:** Creates materials like "ogre01", "ogre11"
- **Second call:** Creates materials like "ogre02", "ogre12"
- **Third call:** Creates materials like "ogre03", "ogre13"

Each call to `LoadMeshesFromDir` creates materials with a new suffix, allowing multiple instances of the same model to have independent materials.

**Serialization Implications:**

- Saved scene references "ogre01" (from first call)
- During scene load, assets persist (thanks to `IsLoadingScene` flag)
- Deserialization finds "ogre01" successfully
- Counter persists across scene loads (it's static), so new `LoadMeshesFromDir` calls create "ogre02", etc.

### Best Practices for Component Asset Management

1. **Use Global Assets When Possible:**
   ```cpp
   // Preferred for shared assets
   xModel.AddMeshEntry(
       Zenith_AssetHandler::GetMesh("Sphere_Smooth"),
       Zenith_AssetHandler::GetMaterial("Crystal")
   );
   ```

2. **Use LoadMeshesFromDir for Unique Models:**
   ```cpp
   // For models with unique materials/textures
   xModel.LoadMeshesFromDir("Meshes/ogre");
   ```

3. **Track Asset Ownership Carefully:**
   - Assets created by `LoadMeshesFromDir` → tracked in `m_xCreatedXXX` → deleted by destructor (except during scene loads)
   - Assets from `AssetHandler::GetXXX()` → NOT tracked → NOT deleted by component
   - **Don't mix ownership models!** If you call `LoadMeshesFromDir`, don't manually add global assets to the same component

4. **Serialization is Automatic:**
   - `WriteToDataStream` automatically saves asset names
   - `ReadFromDataStream` automatically looks up assets by name
   - No manual tracking needed - just ensure assets exist before calling `LoadFromFile`

5. **Clean Up Textures from Deserialized Materials:**
   - When `ReadFromDataStream` creates materials and calls `pxMaterial->ReadFromDataStream()`, textures are loaded from paths
   - These textures allocate slots in `Zenith_AssetHandler` that MUST be freed
   - **CRITICAL:** Call `pxMaterial->DeleteLoadedTextures()` BEFORE `DeleteMaterial()` in your destructor
   - Example from `Zenith_ModelComponent`:
     ```cpp
     for (uint32_t u = 0; u < m_xCreatedMaterials.GetSize(); u++)
     {
         m_xCreatedMaterials.Get(u)->DeleteLoadedTextures();  // Free texture slots first!
         Zenith_AssetHandler::DeleteMaterial(m_xCreatedMaterials.Get(u));
     }
     ```
   - Failure to do this causes texture pool exhaustion on repeated scene reloads

---

## Physics Mesh Generation System

### Overview

The physics mesh generation system automatically creates optimized collision geometry for `Zenith_ModelComponent` entities. This allows visual models to have physically accurate collision shapes without manually authoring collision meshes.

### Architecture

**Key Files:**
- `Zenith/Physics/Zenith_PhysicsMeshGenerator.h` - Generator interface and config
- `Zenith/Physics/Zenith_PhysicsMeshGenerator.cpp` - Generation algorithms (~800 lines)
- `Zenith/EntityComponent/Components/Zenith_ModelComponent.h` - Integration API
- `Zenith/EntityComponent/Components/Zenith_ModelComponent.cpp` - Auto-generation hook

### Quality Levels

The system provides three quality levels with different performance/accuracy trade-offs:

#### LOW Quality - Axis-Aligned Bounding Box (AABB)
```cpp
PhysicsMeshQuality::LOW
```
- **Algorithm:** Computes tight bounding box around all vertices
- **Output:** 8 vertices, 12 triangles (box mesh)
- **Generation Time:** ~1-5ms
- **Memory:** Minimal (96 bytes vertex data)
- **Use Cases:** Simple props, performance-critical scenarios, distant LODs

#### MEDIUM Quality - Convex Hull
```cpp
PhysicsMeshQuality::MEDIUM
```
- **Algorithm:** Quickhull 3D convex hull construction
- **Output:** Varies (minimal convex mesh containing all points)
- **Generation Time:** ~10-50ms
- **Memory:** Depends on mesh complexity
- **Use Cases:** Most gameplay objects, general-purpose collision

#### HIGH Quality - Simplified Mesh
```cpp
PhysicsMeshQuality::HIGH
```
- **Algorithm:** Spatial hashing-based vertex decimation
- **Output:** ~100% of original triangles (configurable via `m_fSimplificationRatio`)
- **Generation Time:** ~50-200ms
- **Memory:** Proportional to original mesh
- **Use Cases:** Complex shapes requiring accurate collision, terrain, architecture

### Configuration

**Global Configuration:**
```cpp
// In Zenith_PhysicsMeshGenerator.h
extern PhysicsMeshConfig g_xPhysicsMeshConfig;

struct PhysicsMeshConfig
{
    PhysicsMeshQuality m_eQuality = PhysicsMeshQuality::MEDIUM;
    float m_fSimplificationRatio = 1.0f;  // 1.0 = 100% (no decimation)
    uint32_t m_uMinTriangles = 100;
    uint32_t m_uMaxTriangles = 10000;
    bool m_bAutoGenerate = true;  // Auto-generate on LoadMeshesFromDir()
    bool m_bEnableDebugLogging = true;
};
```

**Per-Model Override:**
```cpp
PhysicsMeshConfig xCustomConfig;
xCustomConfig.m_eQuality = PhysicsMeshQuality::HIGH;
xCustomConfig.m_fSimplificationRatio = 0.5f;  // 50% of triangles
xModel.GeneratePhysicsMeshWithConfig(xCustomConfig);
```

### Integration Flow

```
1. Entity Creation
   └─→ entity.AddComponent<Zenith_ModelComponent>()

2. Mesh Loading
   └─→ xModel.LoadMeshesFromDir("path/to/meshes")
       │
       ├─→ Loads .zmsh files from directory
       │   └─→ CRITICAL: Must retain position data for physics mesh generation
       │       └─→ Automatically sets: uRetainAttributeBits |= (1 << FLUX_VERTEX_ATTRIBUTE__POSITION)
       │
       └─→ If g_xPhysicsMeshConfig.m_bAutoGenerate == true:
           └─→ Calls GeneratePhysicsMesh()

3. Physics Mesh Generation
   └─→ Zenith_PhysicsMeshGenerator::GeneratePhysicsMeshWithConfig()
       │
       ├─→ Combines all submeshes into unified mesh
       ├─→ Selects algorithm based on quality level
       ├─→ Generates optimized collision mesh
       └─→ Stores in m_pxPhysicsMesh

4. Collider Creation
   └─→ entity.AddComponent<Zenith_ColliderComponent>()
       └─→ AddCollider(COLLISION_VOLUME_TYPE_MODEL_MESH, ...)
           │
           ├─→ Retrieves m_pxPhysicsMesh from ModelComponent
           ├─→ Applies transform scale to vertices (physics mesh is in model space)
           └─→ Creates JPH::ConvexHullShape or JPH::MeshShape for Jolt Physics
```

### Scale Handling

**CRITICAL: Physics mesh vertices are stored in MODEL SPACE (unscaled)**

The system applies scale at different stages:

1. **Physics Mesh Generation:**
   - Vertices stored in model/local space
   - NO scale baked into vertices
   - Entity scale logged but not applied to mesh data

2. **Debug Visualization:**
   - Applies full model matrix (Translation × Rotation × Scale)
   - Uses `BuildModelMatrix()` which includes scale
   - Debug draw shows mesh at correct world-space size

3. **Collider Creation:**
   - Applies scale when copying vertices to Jolt Physics
   - Scales vertices during JPH::ConvexHullShape or JPH::MeshShape construction
   - Jolt Physics doesn't support runtime non-uniform scaling on bodies

**Example - Scale Flow:**
```cpp
// 1. Set scale BEFORE loading meshes
xTransform.SetScale({0.1f, 0.1f, 0.1f});  // Scale entity to 10% size

// 2. Load meshes (auto-generates physics mesh)
xModel.LoadMeshesFromDir("Meshes/Sponza");
// → Physics mesh vertices in model space: e.g., vertex at (100, 50, 20)
// → Entity scale: (0.1, 0.1, 0.1)

// 3. Debug draw (visualization)
xModel.DebugDrawPhysicsMesh();
// → Applies transform: vertex (100, 50, 20) × scale (0.1, 0.1, 0.1) = (10, 5, 2) world space
// → Appears at correct size matching rendered mesh

// 4. Collider creation (physics simulation)
xEntity.AddComponent<Zenith_ColliderComponent>();
xCollider.AddCollider(COLLISION_VOLUME_TYPE_MODEL_MESH, ...);
// → Applies scale during Jolt shape creation
// → Physics body has correctly scaled collision shape
```

### Vertex Data Retention

**CRITICAL: Position data must be retained in CPU memory for physics mesh generation**

By default, `LoadMeshesFromDir` uploads mesh data to GPU and discards CPU copies. Physics mesh generation requires CPU access to vertex positions.

**Solution:**
```cpp
// In Zenith_ModelComponent.h - LoadMeshesFromDir()
void LoadMeshesFromDir(const std::filesystem::path& strPath, ...)
{
    // ...
    
    // If physics mesh auto-generation is enabled, ensure position data is retained
    if (g_xPhysicsMeshConfig.m_bAutoGenerate)
    {
        uRetainAttributeBits |= (1 << FLUX_VERTEX_ATTRIBUTE__POSITION);
    }
    
    // Load mesh with retained position data
    Zenith_AssetHandler::AddMesh(name, path, uRetainAttributeBits, bUploadToGPU);
}
```

**Without this fix:**
- Physics mesh generator sees 0 vertices
- Falls back to unit box (AABB: -0.5 to 0.5)
- All models have identical box collision shapes
- Log shows: `Not enough vertices for convex hull (0), using AABB fallback`

**With this fix:**
- Physics mesh generator accesses actual vertex positions
- Generates accurate collision shapes
- Models have properly fitted collision meshes
- Log shows: `Generated physics mesh: 1234 verts, 2468 tris`

### Debug Visualization

**Enabling Debug Draw:**
```cpp
// Per-model toggle
xModel.SetDebugDrawPhysicsMesh(true);

// Global toggle for all models
g_bDebugDrawAllPhysicsMeshes = true;
```

**Rendering:**
```cpp
// Called automatically in model's render pass
void Zenith_ModelComponent::DebugDrawPhysicsMesh()
{
    if (!m_bDebugDrawPhysicsMesh || !m_pxPhysicsMesh)
        return;
    
    // Get full transform matrix (includes scale!)
    Zenith_Maths::Matrix4 xModelMatrix;
    xTransform.BuildModelMatrix(xModelMatrix);
    
    // Draw wireframe in world space
    Zenith_PhysicsMeshGenerator::DebugDrawPhysicsMesh(
        m_pxPhysicsMesh, 
        xModelMatrix, 
        m_xDebugDrawColor  // Default: green (0, 1, 0)
    );
}
```

**Implementation:**
- Uses `Flux_Primitives::AddLine()` to draw edges
- Wireframe color customizable via `m_xDebugDrawColor`
- Transforms vertices from model space to world space
- Draws all triangle edges (3 lines per triangle)

### Debug Logging

**Log Tags:**
- `[ModelPhysics]` - ModelComponent physics mesh operations
- `[PhysicsMeshGen]` - Physics mesh generator algorithm details
- `[Collider]` - Collider creation and Jolt Physics integration

**Example Output:**
```
[ModelPhysics] Generating physics mesh with entity scale (0.100, 0.100, 0.100)
[PhysicsMeshGen] Generating physics mesh from 103 submeshes (12456 verts, 24912 tris), quality=MEDIUM (ConvexHull)
[PhysicsMeshGen] Generated convex hull: 234 verts, 468 tris
[ModelPhysics] Generated physics mesh for model: 234 verts, 468 tris
[ModelPhysics] First vertex in model space: (123.456, 67.890, 45.123)
[ModelPhysics] DebugDraw: Entity scale (0.100, 0.100, 0.100), verts=234
[Collider] Creating convex hull with scale (0.100, 0.100, 0.100), 234 points
[Collider] Created convex hull collider successfully
```

### Known Limitations

1. **Animation Not Supported:**
   - Physics mesh generated from T-pose/bind pose
   - Does NOT update during skeletal animation
   - Collision shape remains static even if mesh deforms
   - **Workaround:** Use compound shapes or multiple colliders for articulated objects

2. **Non-Uniform Scale Performance:**
   - Jolt Physics doesn't support runtime non-uniform scaling
   - Scale must be baked into vertices during collider creation
   - Changing scale requires regenerating physics mesh and rebuilding collider

3. **Generation Time:**
   - HIGH quality can take 50-200ms for complex meshes
   - Blocks main thread during generation
   - **Future:** Move to background thread with async loading

4. **Memory Overhead:**
   - Requires retaining position data in CPU memory
   - Adds ~12 bytes per vertex (3 floats)
   - **Optimization:** Could discard after physics mesh generation if not needed

### API Reference

**Zenith_ModelComponent Methods:**
```cpp
// Generate with global config
void GeneratePhysicsMesh(PhysicsMeshQuality eQuality = g_xPhysicsMeshConfig.m_eQuality);

// Generate with custom config
void GeneratePhysicsMeshWithConfig(const PhysicsMeshConfig& xConfig);

// Clear existing physics mesh
void ClearPhysicsMesh();

// Query
bool HasPhysicsMesh() const;
Flux_MeshGeometry* GetPhysicsMesh() const;

// Debug visualization
void SetDebugDrawPhysicsMesh(bool bEnable);
void DebugDrawPhysicsMesh();
void SetPhysicsMeshDebugColor(const Zenith_Maths::Vector3& xColor);
```

**Zenith_PhysicsMeshGenerator Static Methods:**
```cpp
// Generate from multiple submeshes
static Flux_MeshGeometry* GeneratePhysicsMesh(
    const Zenith_Vector<Flux_MeshGeometry*>& xMeshGeometries,
    PhysicsMeshQuality eQuality);

// Generate with custom config
static Flux_MeshGeometry* GeneratePhysicsMeshWithConfig(
    const Zenith_Vector<Flux_MeshGeometry*>& xMeshGeometries,
    const PhysicsMeshConfig& xConfig);

// Debug visualization
static void DebugDrawPhysicsMesh(
    const Flux_MeshGeometry* pxPhysicsMesh,
    const Zenith_Maths::Matrix4& xTransform,
    const Zenith_Maths::Vector3& xColor);

// Utility
static const char* GetQualityName(PhysicsMeshQuality eQuality);
```

### Performance Recommendations

1. **Use MEDIUM for most objects:**
   - Good balance of accuracy and performance
   - Suitable for 90% of use cases

2. **Use LOW for:**
   - Simple geometric shapes (boxes, spheres already primitive)
   - Distant objects where precision doesn't matter
   - Performance-critical scenarios (hundreds of objects)

3. **Use HIGH sparingly:**
   - Complex architectural geometry
   - Terrain collision meshes
   - Objects where collision accuracy is gameplay-critical

4. **Optimize mesh loading:**
   - Only retain position data when needed
   - Consider async generation for large meshes
   - Cache generated physics meshes to disk

---

## Conclusion

Zenith's ECS provides:
- **Simplicity:** Easy to understand and use
- **Performance:** Cache-friendly component storage
- **Flexibility:** Components define entity behavior
- **Type Safety:** Compile-time type checking
- **Extensibility:** Easy to add new component types
- **Thread Safety:** Deferred scene loading prevents concurrent access
- **Physics Integration:** Automatic collision mesh generation

While not as optimized as pure archetype-based ECS (like EnTT or FLECS), it strikes a balance between performance and simplicity, suitable for games with thousands of entities.
