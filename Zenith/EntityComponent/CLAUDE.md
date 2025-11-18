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
```

**Correct load order:**
```
Load order: Transform → Model → Camera → Text → Terrain → Collider
                                                  ↑         ↑
                                              Loaded 5th  Loaded 6th

  1. Transform ✅
  2. Model ✅
  3. Camera ✅
  4. Text ✅
  5. Terrain ✅
  6. Collider → ReadFromDataStream() → AddCollider(TERRAIN)
              → Checks for TerrainComponent... ✅ EXISTS!
              → Uses terrain mesh data
              → ✅ SUCCESS!
```

**General Rule:** When adding new components:
1. **Identify dependencies:** Does the component's constructor or `ReadFromDataStream` access other components?
2. **Update serialization order:** Ensure dependencies are serialized BEFORE dependent components
3. **Test loading:** Verify scene loads without assertions

**Common Dependency Chains:**
- `ColliderComponent` → depends on → `TerrainComponent` (for terrain colliders)
- `ScriptComponent` → may depend on → any component (check script implementation)
- `ChildComponent` → would depend on → `ParentComponent` (hypothetical example)

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

## Conclusion

Zenith's ECS provides:
- **Simplicity:** Easy to understand and use
- **Performance:** Cache-friendly component storage
- **Flexibility:** Components define entity behavior
- **Type Safety:** Compile-time type checking
- **Extensibility:** Easy to add new component types
- **Thread Safety:** Deferred scene loading prevents concurrent access

While not as optimized as pure archetype-based ECS (like EnTT or FLECS), it strikes a balance between performance and simplicity, suitable for games with thousands of entities.
