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
    void SetScript(Zenith_Script* script);
    void Update(float dt);
    
private:
    Zenith_Script* m_pxScript;
    Zenith_Entity m_xParentEntity;
};

// User-defined script
class MyScript : public Zenith_Script {
    void OnCreate(Zenith_Entity& entity) override {
        // Initialization
    }
    
    void OnUpdate(float dt, Zenith_Entity& entity) override {
        // Per-frame logic
        if (Zenith_Input::IsKeyPressed(KEY_W)) {
            auto& transform = entity.GetComponent<TransformComponent>();
            transform.m_xPosition.z += 5.0f * dt;
        }
    }
};
```

### Zenith_ColliderComponent

**Purpose:** Physics collision shape

```cpp
class Zenith_ColliderComponent {
public:
    enum ColliderType {
        COLLIDER_TYPE_BOX,
        COLLIDER_TYPE_SPHERE,
        COLLIDER_TYPE_CAPSULE,
        COLLIDER_TYPE_MESH
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
    
    // ReactPhysics3D handle
    reactphysics3d::RigidBody* m_pxRigidBody;
    reactphysics3d::Collider* m_pxCollider;
    
private:
    Zenith_Entity m_xParentEntity;
};
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

### Physics System Integration

```cpp
void Zenith_Physics::Update(float dt) {
    // Step physics simulation
    m_pxWorld->update(dt);
    
    // Sync physics transforms to entity transforms
    Zenith_Vector<Zenith_ColliderComponent*> colliders;
    Zenith_Scene::GetCurrentScene().GetAllOfComponentType<Zenith_ColliderComponent>(colliders);
    
    for (Zenith_ColliderComponent* collider : colliders) {
        if (collider->m_bIsKinematic)
            continue;  // Kinematic objects driven by entity transform
        
        // Get physics transform
        const reactphysics3d::Transform& physicsTransform = 
            collider->m_pxRigidBody->getTransform();
        
        // Update entity transform
        Zenith_Entity& entity = collider->m_xParentEntity;
        Zenith_TransformComponent& transform = entity.GetComponent<TransformComponent>();
        
        transform.m_xPosition = ConvertVector(physicsTransform.getPosition());
        transform.m_xRotation = ConvertQuaternion(physicsTransform.getOrientation());
    }
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

## Conclusion

Zenith's ECS provides:
- **Simplicity:** Easy to understand and use
- **Performance:** Cache-friendly component storage
- **Flexibility:** Components define entity behavior
- **Type Safety:** Compile-time type checking
- **Extensibility:** Easy to add new component types

While not as optimized as pure archetype-based ECS (like EnTT or FLECS), it strikes a balance between performance and simplicity, suitable for games with thousands of entities.
