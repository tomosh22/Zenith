# Zenith Game Engine - Architecture Overview

## Introduction

Zenith is a high-performance, data-oriented game engine written in C++20. It features a custom Entity-Component System (ECS), a Vulkan-based renderer (Flux), and a task system for efficient multi-threaded execution.

## Core Philosophy

**Design Principles:**
- **Multi-Threading First:** All major systems designed for parallel execution
- **Minimal Dependencies:** Self-contained with only essential external libraries
- **Deterministic Behavior:** Predictable execution for debugging and replication

## High-Level Architecture

```
???????????????????????????????????????????????????????????????
?                    Application Layer                        ?
?  ??????????????  ??????????????  ???????????????            ?
?  ?   Game     ?  ?   Editor   ?  ?    Tools    ?            ?
?  ?   Logic    ?  ?   (ImGui)  ?  ?             ?            ?
?  ??????????????  ??????????????  ???????????????            ?
???????????????????????????????????????????????????????????????
                            ?
???????????????????????????????????????????????????????????????
?                      Scene Layer                            ?
?  ????????????????????????????????????????????????????????   ?
?  ?  Entity-Component System (Zenith_Scene)              ?   ?
?  ?  � Entities: Lightweight IDs                        ?   ?
?  ?  � Components: (Transform, Model, etc.)             ?   ?
?  ?  � Systems: Logic operators on components           ?   ?
?  ????????????????????????????????????????????????????????   ?
???????????????????????????????????????????????????????????????
                            ?
???????????????????????????????????????????????????????????????
?                     Systems Layer                           ?
? ???????????? ???????????? ???????????? ????????????????     ?
? ? Physics  ? ?  Input   ? ?  Audio   ? ? Asset        ?     ?
? ?  (Jolt)  ? ?  System  ? ?  System  ? ? Management   ?     ?
? ???????????? ???????????? ???????????? ????????????????     ?
???????????????????????????????????????????????????????????????
                            ?
???????????????????????????????????????????????????????????????
?                    Rendering Layer (Flux)                   ?
?  ??????????????????????????????????????????????????????     ?
?  ?  Deferred Rendering Pipeline                       ?     ?
?  ?  � G-Buffer Pass                                  ?     ?
?  ?  � Shadow Maps (Cascaded Shadow Maps)             ?     ?
?  ?  � Lighting Pass                                  ?     ?
?  ?  � Post-Processing (SSAO, Fog, etc.)              ?     ?
?  ?  � Forward Pass (Transparency, Particles)         ?     ?
?  ??????????????????????????????????????????????????????     ?
???????????????????????????????????????????????????????????????
                            ?
???????????????????????????????????????????????????????????????
?                   Task System Layer                         ?
?  ????????????????????????????????????????????????????????   ?
?  ?  Zenith_TaskSystem (Work-Stealing)                   ?   ?
?  ?  � Thread Pool                                      ?   ?
?  ?  � Task Graph Support                               ?   ?
?  ?  � Profiling Integration                            ?   ?
?  ????????????????????????????????????????????????????????   ?
???????????????????????????????????????????????????????????????
                            ?
???????????????????????????????????????????????????????????????
?                   Platform Layer                            ?
?  ????????????  ????????????  ????????????                   ?
?  ? Windows  ?  ?  Vulkan  ?  ?   GLFW   ?                   ?
?  ?  API     ?  ?   API    ?  ? (Window) ?                   ?
?  ????????????  ????????????  ????????????                   ?
???????????????????????????????????????????????????????????????
                            ?
???????????????????????????????????????????????????????????????
?                   Core Utilities                            ?
?  � Memory Management  � Collections  � Math Library       ?
?  � Profiling         � Data Streams  � String Utilities   ?
???????????????????????????????????????????????????????????????
```

## Major Components

### 1. Entity-Component System (ECS)

**Location:** `Zenith/EntityComponent/`

The heart of Zenith's game object model. See [EntityComponent/CLAUDE.md](Zenith/EntityComponent/CLAUDE.md) for detailed documentation.

**Key Features:**
- Type-safe component pools with contiguous storage
- Efficient component iteration for systems
- Parent-child entity hierarchies
- Scene serialization/deserialization

**Common Components:**
- `Zenith_TransformComponent` - Position, rotation, scale
- `Zenith_ModelComponent` - Renderable mesh
- `Zenith_CameraComponent` - View/projection matrices
- `Zenith_ColliderComponent` - Physics collision shapes
- `Zenith_ScriptComponent` - Custom behavior attachment

### 2. Flux Renderer

**Location:** `Zenith/Flux/`

Modern Vulkan-based deferred renderer with multi-threaded command buffer recording.

**Architecture:**
- **Command List System:** Platform-agnostic rendering commands
- **Deferred Pipeline:** G-Buffer ? Lighting ? Post-Processing
- **Multi-threaded:** Parallel command buffer recording across worker threads
- **Render Graph:** Automatic resource barrier insertion

**Render Passes:**
1. Shadow Map Generation (Cascaded Shadow Maps)
2. G-Buffer Pass (Diffuse, Normals, Material Properties)
3. Deferred Lighting
4. Screen Space Ambient Occlusion (SSAO)
5. Fog Application
6. Forward Rendering (Particles, UI, etc.)
7. Post-Processing

**Key Files:**
- `Flux.h/cpp` - Core rendering infrastructure
- `Flux_Graphics.h/cpp` - Global graphics state
- `Flux_CommandList.h/cpp` - Platform-agnostic command recording

**Terrain Frustum Culling:**
- **Location:** `Zenith/Flux/Terrain/`
- **Documentation:** See [Flux/Terrain/CLAUDE.md](Zenith/Flux/Terrain/CLAUDE.md)
- CPU-based AABB frustum culling for terrain components
- GPU culling infrastructure prepared (compute shader ready, currently disabled)
- 70-75% reduction in terrain rendered vs distance-based checks
- ~30-50 CPU cycles per terrain chunk
- Lazy AABB caching in TerrainComponent (24 bytes per terrain)
- Conservative culling (no false negatives)

### 3. Task System

**Location:** `Zenith/TaskSystem/`

Parallel task scheduler for efficient CPU utilization.

**Features:**
- **Profiling Integration:** Per-task timing and thread tracking

**Usage Pattern:**
```cpp
// Define task function
void MyTaskFunction(void* pData) {
    // Do work...
}

// Create and submit task
Zenith_Task* pTask = new Zenith_Task(
    ZENITH_PROFILE_INDEX__MY_SYSTEM,
    MyTaskFunction,
    pData
);
Zenith_TaskSystem::SubmitTask(pTask);

// Wait for completion
pTask->WaitUntilComplete();
```

### 4. Physics System

**Location:** `Zenith/Physics/`

Integration with ReactPhysics3D for rigid body dynamics.

**Features:**
- Rigid body simulation
- Collision detection (box, sphere, capsule, mesh)
- Constraint solving (hinges, sliders, etc.)
- Raycasting and shape casting
- Character controller

**Integration:**
- Physics bodies synced with `Zenith_TransformComponent`
- `Zenith_ColliderComponent` manages ReactPhysics3D handles
- Physics update runs in parallel with rendering

### 5. Asset Management

**Location:** `Zenith/AssetHandling/`

Runtime asset loading and management system.

**Supported Formats:**
- **Meshes:** Custom `.zmsh` format
- **Textures:** Custom `.ztx` format
- **Animations:** Skeletal animation data

**Asset Pipeline:**
1. **Tools:** Convert from industry formats (FBX, PNG, etc.) to Zenith formats
2. **Runtime:** Fast loading of pre-processed assets

### 6. Profiling System

**Location:** `Zenith/Profiling/`

Hierarchical CPU profiling with visualization via ImGUI.

**Features:**
- Per-thread event tracking
- Hierarchical profile scopes
- ImGui visualization

**Usage:**
```cpp
void MyFunction() {
    ZENITH_PROFILING_FUNCTION_WRAPPER(
        MyExpensiveOperation,
        ZENITH_PROFILE_INDEX__MY_OPERATION,
        param1, param2
    );
}
```

### 7. Memory Management

**Location:** `Zenith/Core/Memory/`

Custom memory allocator.

**Features:**
- Leak detection in debug builds
- Tracking of allocation sources

### 8. Collections

**Location:** `Zenith/Collections/`

Custom container implementations optimized for performance.

**Containers:**
- `Zenith_Vector<T>` - Dynamic array with configurable growth
- `Zenith_MemoryPool<T>` - Fixed-capacity object pool

**Design:**
- Contiguous memory layouts where possible
- Explicit capacity control
- Move semantics support

### 9. Math Library

**Location:** `Zenith/Maths/`

Wrapper around GLM with engine-specific extensions.

**Types:**
- `Zenith_Maths::Vector2/3/4` - Vector types
- `Zenith_Maths::Matrix3/4` - Matrix types
- `Zenith_Maths::Quaternion` - Rotation representation

**Features:**
- Handedness: Left-handed coordinate system

## Frame Loop

The main game loop in `Zenith_Core::Zenith_MainLoop()`:

```cpp
void Zenith_Core::Zenith_MainLoop() {
    // 1. Begin Frame
    UpdateTimers();                    // Calculate delta time
    Zenith_Input::BeginFrame();        // Poll input
    Flux_MemoryManager::BeginFrame();  // Reset frame allocators
    Flux_Swapchain::BeginFrame();      // Acquire swapchain image

#ifdef ZENITH_TOOLS
    // 2. Update Editor (CRITICAL: Before any rendering!)
    Zenith_Editor::Update();           // Process deferred scene loads, etc.
#endif

    // 3. Update Game Logic
    Zenith_Physics::Update(dt);        // Physics simulation
    Zenith_Scene::Update(dt);          // Entity scripts

    // 4. Upload Frame Data
    Flux_Graphics::UploadFrameConstants();  // Camera matrices, etc.

    // 5. Submit Render Tasks
    SubmitRenderTasks();               // Queue rendering work

    // 6. Wait for Completion
    WaitForRenderTasks();              // Sync with GPU

    // 7. Present
    Flux_Swapchain::EndFrame();        // Present to screen
}
```

**Execution Model:**
- Game logic runs on main thread
- Rendering runs on worker threads

**CRITICAL: Editor Update Timing**

`Zenith_Editor::Update()` MUST be called before any rendering for thread safety:

1. **Scene Loading:** When user clicks "Open Scene" in the editor menu, a flag is set but the scene is NOT loaded immediately (that would happen during `SubmitRenderTasks()`). Instead, the load is deferred to the next frame's `Update()` call.

2. **Why This Matters:** `Zenith_Scene::LoadFromFile()` calls `Reset()`, which destroys all component pools. If this happens while render tasks are active (reading from those pools), the engine crashes due to concurrent access.

3. **Safe Timeline:**
   ```
   Frame N: User clicks "Open Scene"
     └─ Sets s_bPendingSceneLoad flag (during RenderImGui)
     └─ Render tasks complete

   Frame N+1: Scene load actually happens
     └─ Zenith_Editor::Update() checks flag
     └─ Calls LoadFromFile() BEFORE any render tasks start
     └─ Safe: no tasks accessing scene data
   ```

4. **Integration Point:** `Zenith_Editor::Update()` is called at [Zenith_Core.cpp:181](Zenith/Core/Zenith_Core.cpp#L181), immediately after frame setup and before any game logic or rendering.

See [EntityComponent/CLAUDE.md - Thread Safety](Zenith/EntityComponent/CLAUDE.md#thread-safety-and-concurrency) for detailed documentation of the scene loading architecture.

## Multithreading Strategy

### Worker Threads

The engine spawns `ZENITH_NUM_WORKER_THREADS` threads at startup:

```cpp
constexpr u_int ZENITH_NUM_WORKER_THREADS = 8;
```

**Thread Roles:**
1. **Main Thread:** Game logic, ECS updates, input handling
2. **Worker Threads 0-7:** Task execution (rendering, physics, etc.)

### Synchronization

**Mutexes:** Used sparingly, primarily for:
- Scene modifications (adding/removing entities)
- Asset loading
- Command list submission

**Lock-Free:** Preferred where possible:
- Task queue (atomic operations)
- Frame allocators (thread-local)
- Component iteration (read-only during rendering)

### Frame Pipelining

The engine can pipeline up to MAX_FRAMES_IN_FLIGHT frames:
- Frame N: GPU execution
- Frame N+1: CPU preparation

**Benefit:** Reduced CPU-GPU bubbles, higher throughput

## Build System

**Build Tool:** Sharpmake (C# based meta-build system)

**Configuration:**
```csharp
AddTargets(new CustomTarget {
    Platform = Platform.win64,
    DevEnv = DevEnv.vs2022,
    Optimization = Optimization.Debug | Optimization.Release,
    ToolsEnabled = ToolsEnabled.True | ToolsEnabled.False
});
```

**Outputs:**
- Game executable (with/without tools)
- Editor executable (tools enabled)
- Asset conversion tools

## Directory Structure

```
Zenith/
??? AssetHandling/      # Asset loading and management
??? Collections/        # Custom containers (Vector, Pool, etc.)
??? Core/              # Core utilities and engine loop
?   ??? Memory/        # Memory allocators
?   ??? Zenith_Core.*  # Main game loop
??? DataStream/        # Binary serialization
??? DebugVariables/    # Runtime-tweakable debug variables
??? EntityComponent/   # ECS implementation
?   ??? Components/    # Component definitions
??? Flux/              # Vulkan renderer
?   ??? StaticMeshes/  # Opaque geometry rendering
?   ??? Shadows/       # Shadow map generation
?   ??? DeferredShading/ # Lighting pass
?   ??? Particles/     # Particle systems
?   ??? Shaders/       # GLSL shader source
??? Input/             # Keyboard/mouse/gamepad input
??? Maths/             # Math library (GLM wrapper)
??? Physics/           # ReactPhysics3D integration
??? Profiling/         # CPU profiler
??? StateMachine/      # Game state management
??? TaskSystem/        # Multi-threading infrastructure
??? Vulkan/            # Vulkan platform backend
??? Windows/           # Windows platform layer

Games/
??? SuperSecret/       # Example game project
?   ??? Assets/       # Game-specific assets
?   ??? Source/       # Game code

Tools/
??? FluxCompiler/      # Shader compilation tool
??? AssetConverters/   # Mesh/texture converters
```

## Platform Support

**Current Platforms:**
- Windows 10/11 (x64)

**Planned:**
- Linux (Ubuntu 20.04+)
- macOS (Metal backend)

**Graphics API:**
- Vulkan 1.3 (primary)
- DirectX 12 (planned)

## Dependencies

**Core Engine:**
- GLM - Math library
- Jolt - Physics simulation
- GLFW - Window/input management
- Vulkan SDK - Graphics API
- - ImGui - Debug UI


**Tools Only:**
- Assimp - Model importing
- OpenCV - Image processing

## Performance Characteristics

**Target:**
- 60 FPS @ 1080p on mid-range hardware
- 10,000+ entities with active physics
- 100+ dynamic lights with shadows

**Bottlenecks:**
- GPU: Shadow map rendering (4 cascades)
- CPU: Physics broad phase (O(n log n))
- Memory: Texture streaming

**Optimizations:**
- Occlusion culling (planned)
- Level-of-detail (planned)
- Texture streaming (planned)

## Debug Features

**Tools Build (`ZENITH_TOOLS` defined):**
- ImGui editor interface
- Visual profiler
- Entity inspector

**Release Build:**
- Minimal overhead
- No debug variables
- Optimized shaders

## Getting Started

### Building

```bash
cd Games/SuperSecret/Build
sharpmake /sources("Sharpmake.cs")
# Open generated solution in Visual Studio
```

### Creating an Entity

```cpp
// Create entity
Zenith_Entity entity = scene.CreateEntity("MyEntity");

// Add components
auto& transform = entity.AddComponent<Zenith_TransformComponent>(
    Vector3(0, 0, 0),  // position
    Quaternion::identity(),  // rotation
    Vector3(1, 1, 1)   // scale
);

auto& model = entity.AddComponent<Zenith_ModelComponent>();
model.m_pxModel = AssetHandler::GetModel("MyMesh");
model.m_pxMaterial = AssetHandler::GetMaterial("MyMaterial");
```

### Custom Component

```cpp
class MyComponent {
public:
    MyComponent(Zenith_Entity& entity)
        : m_xParentEntity(entity) {}
    
    void Update(float dt) {
        // Custom logic
    }
    
private:
    Zenith_Entity m_xParentEntity;
    float m_fCustomData;
};

// Register with scene
entity.AddComponent<MyComponent>();
```

## Best Practices

### Performance

1. **Batch Similar Work:** Group similar entities for better cache usage
2. **Avoid Pointer Chasing:** Prefer indices over pointers
3. **Profile First:** Use the built-in profiler before optimizing
4. **Minimize Allocations:** Use frame allocators for temporary data

### Architecture

1. **Prefer Composition:** Build complex behavior from simple components
2. **Document Public APIs:** All public headers should have comments

### Debugging

1. **Use Debug Variables:** Expose tunables via `Zenith_DebugVariables`
2. **Profile Suspect Code:** Add profile scopes to narrow issues
3. **Check Assertions:** `Zenith_Assert` will break in debug builds
4. **Visualize Data:** Use ImGui for runtime inspection

## Common Patterns

### Singleton Access

```cpp
// Scene
Zenith_Scene& scene = Zenith_Scene::GetCurrentScene();

// Input
if (Zenith_Input::IsKeyPressed(KEY_W)) { ... }

// Time
float dt = Zenith_Core::GetDt();
```

### Resource Loading

```cpp
// Async load (returns immediately)
AssetHandler::LoadTextureAsync("MyTexture.ztx", [](Texture* pTex) {
    // Callback on completion
});

// Sync load (blocks until complete)
Texture* pTex = AssetHandler::LoadTexture("MyTexture.ztx");
```

### Task Submission

```cpp
// Simple task
Zenith_Task task(PROFILE_INDEX, MyFunction, pData);
Zenith_TaskSystem::SubmitTask(&task);
task.WaitUntilComplete();

// Task array (parallel for)
Zenith_TaskArray tasks(PROFILE_INDEX, MyFunction, pArray, count);
Zenith_TaskSystem::SubmitTaskArray(&tasks);
tasks.WaitUntilComplete();
```

## Future Roadmap

**Short Term (3-6 months):**
- [ ] Occlusion culling
- [ ] Texture streaming
- [ ] Animation blending
- [ ] Audio system

**Medium Term (6-12 months):**
- [ ] DirectX 12 backend
- [ ] Linux support
- [ ] Networked multiplayer
- [ ] Asset hot-reloading improvements

**Long Term (12+ months):**
- [ ] Ray tracing support
- [ ] Advanced particle systems
- [ ] Procedural generation tools
- [ ] Full editor suite


## License

[To be determined]

---

*Last Updated: 2025*
*Engine Version: Development Build*
*Author: Zenith Engine Team*
