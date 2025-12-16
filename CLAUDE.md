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
- `Zenith_ScriptComponent` - Custom behavior attachment (uses behavior registry for serialization)
- `Zenith_TerrainComponent` - Heightmap-based terrain with 64×64 physics mesh chunks

**ScriptComponent & Behavior Registry (December 2024):**
ScriptComponent uses a factory pattern (`Zenith_BehaviourRegistry`) to serialize/deserialize polymorphic behaviors.
All game behaviors must use the `ZENITH_BEHAVIOUR_TYPE_NAME(ClassName)` macro and register at startup.
See [EntityComponent/CLAUDE.md - ScriptComponent](Zenith/EntityComponent/CLAUDE.md#zenith_scriptcomponent) for details.

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
- **Documentation:** See [Flux/Terrain/CLAUDE.md](Zenith/Flux/Terrain/CLAUDE.md) - **CRITICAL: READ BEFORE MODIFYING**
- CPU-based AABB frustum culling for terrain components
- GPU culling infrastructure prepared (compute shader ready, currently disabled)
- 70-75% reduction in terrain rendered vs distance-based checks
- ~30-50 CPU cycles per terrain chunk
- Lazy AABB caching in TerrainComponent (24 bytes per terrain)
- Conservative culling (no false negatives)

**Terrain LOD Streaming:**
- **Location:** `Zenith/Flux/Terrain/Flux_TerrainStreamingManager.h/cpp`
- **Documentation:** See [Flux/Terrain/CLAUDE.md](Zenith/Flux/Terrain/CLAUDE.md) - **CRITICAL: READ BEFORE MODIFYING**
- 4096 terrain chunks (64×64 grid), 4 LOD levels (LOD0=highest, LOD3=lowest)
- Unified buffer architecture: LOD3 always-resident + streaming region for LOD0-2
- Streaming budget: 256MB vertices, 64MB indices
- Distance-based LOD selection matching between CPU and GPU
- Priority-queue streaming with LRU eviction
- State machine: NOT_LOADED → QUEUED → LOADING → RESIDENT → EVICTING
- AABBs cached from actual mesh data during initialization

**⚠️ TERRAIN SYSTEM WARNING:**
The terrain LOD streaming system has had several subtle bugs that took hours to debug.
Before making ANY changes, read the "Critical Bug History" section in [Flux/Terrain/CLAUDE.md](Zenith/Flux/Terrain/CLAUDE.md).
Key pitfalls:
- `TERRAIN_SCALE` mismatch between export tool (1) and runtime (8) - use TERRAIN_SIZE only
- CPU/GPU LOD thresholds MUST match exactly (400000, 1000000, 2000000)
- Buffer offsets: absolute vs relative - allocator uses relative, residency stores absolute
- Vertex stride must be 60 bytes everywhere (not 12 for position-only)

**Editor Gizmos (Tools Build Only):**
- **Location:** `Zenith/Flux/Gizmos/`
- **Documentation:** See [Flux/Gizmos/CLAUDE.md](Zenith/Flux/Gizmos/CLAUDE.md) - **CRITICAL: READ BEFORE MODIFYING**
- 3D translation/rotation/scale gizmos rendered via Flux pipeline
- Screen-to-world raycasting with ray-cylinder intersection for axis selection
- Line-line closest point algorithm for translation manipulation
- Auto-scaling based on camera distance for constant screen-space size
- Integration with Editor via `Zenith_Editor.cpp` (see [Editor/CLAUDE.md](Zenith/Editor/CLAUDE.md))

**⚠️ GIZMO SYSTEM WARNING:**
The gizmo interaction system has had several critical bugs involving coordinate spaces and state management.
Before making ANY changes, read the "Critical Bug History" section in [Flux/Gizmos/CLAUDE.md](Zenith/Flux/Gizmos/CLAUDE.md).
Key pitfalls:
- Do NOT call `SetTargetEntity()` during active interaction (resets state)
- Ray origin and direction MUST both be in world space (not mixed local/world)
- No Y-axis inversion in `ScreenToWorldRay()` - projection matrix handles it
- `GIZMO_INTERACTION_LENGTH_MULTIPLIER` must be 1.0 (not larger)

**Debug Primitives (Tools Build Only):**
- **Location:** `Zenith/Flux/Primitives/`
- **Purpose:** Immediate-mode debug rendering for spheres, cubes, lines, wireframes, etc.
- **Render Pipeline:** Forward pass at `RENDER_ORDER_PRIMITIVES`, writes to GBuffer
- **Usage:** `Flux_Primitives::AddLine(start, end, color, thickness)`
- **Auto-clear:** Primitives cleared each frame after rendering
- **Task-based:** Rendering submitted as separate task, executes on worker threads
- **Use Cases:**
  - Physics mesh visualization (green wireframe overlays)
  - Gizmo rendering (3D transform manipulators)
  - Debug drawing (collision shapes, rays, bounding boxes)
- **Performance:** Batched into single draw call per primitive type per frame

**Physics Mesh Visualization (December 2024):**
- Physics meshes rendered as green wireframe via `Flux_Primitives::AddLine()`
- Debug draw called from `Zenith_Core::Zenith_MainLoop()` before render tasks
- Visible in all editor modes (Stopped/Paused/Playing)
- Controlled by `g_xPhysicsMeshConfig.m_bDebugDraw` flag
- Used for verifying physics mesh generation during scene deserialization
- See [Editor/CLAUDE.md - Physics Mesh Debug Visualization](Zenith/Editor/CLAUDE.md#physics-mesh-debug-visualization)

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

Integration with **Jolt Physics** for rigid body dynamics.

**Features:**
- Rigid body simulation
- Collision detection (box, sphere, capsule, mesh)
- Constraint solving
- Raycasting and shape casting
- Character controller

**Integration:**
- Physics bodies synced with `Zenith_TransformComponent`
- `Zenith_ColliderComponent` manages Jolt Physics handles (`JPH::Body*`)
- Fixed timestep simulation (60 Hz)
- Contact listener for collision callbacks

**Key Implementation Details:**
- Uses `JPH::PhysicsSystem` for simulation
- `JPH::JobSystemThreadPool` uses `hardware_concurrency - 1` threads
- `JPH::TempAllocatorImpl` with 10MB pre-allocated
- Two object layers: `NON_MOVING` (static) and `MOVING` (dynamic)
- Two broadphase layers for efficient spatial partitioning

### 5. Asset Management

**Location:** `Zenith/AssetHandling/`

Runtime asset loading and management system.

**Supported Formats:**
- **Meshes:** Custom `.zmsh` format
- **Textures:** Custom `.ztx` format
- **Materials:** Custom `.zmat` format (December 2024)
- **Animations:** Skeletal animation data

**Asset Pipeline:**
1. **Tools:** Convert from industry formats (FBX, PNG, etc.) to Zenith formats
2. **Runtime:** Fast loading of pre-processed assets

**Material System (December 2024):**
Materials aggregate textures and must store texture **source paths** for scene reload to work.
Use `SetDiffuseWithPath()` instead of `SetDiffuse()` when setting up materials.
See [Flux/CLAUDE.md - Material System](Zenith/Flux/CLAUDE.md#flux-material-system-december-2024) for details.

**Asset Lifecycle:**
- Fixed-size pools: `ZENITH_MAX_TEXTURES`, `ZENITH_MAX_MESHES`, `ZENITH_MAX_MATERIALS`
- Slot allocation tracked via `s_xUsedTextureIDs`, etc.
- **⚠️ CRITICAL:** Assets created during scene deserialization MUST be cleaned up by component destructors
- Use `Zenith_Scene::IsLoadingScene()` to check if cleanup should be skipped during reload

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

The main game loop in `Zenith_Core::Zenith_MainLoop()` (verified from source):

```cpp
void Zenith_Core::Zenith_MainLoop() {
    // 1. Begin Frame
    Flux_PlatformAPI::BeginFrame();    // Platform setup
    UpdateTimers();                    // Calculate delta time
    Zenith_Input::BeginFrame();        // Poll input
    Zenith_Window::GetInstance()->BeginFrame();  // Window events
    Flux_MemoryManager::BeginFrame();  // Reset frame allocators
    if (!Flux_Swapchain::BeginFrame()) // Acquire swapchain image
        return;  // Skip frame if swapchain unavailable

#ifdef ZENITH_TOOLS
    // 2. Update Editor (CRITICAL: Before any rendering!)
    Zenith_Editor::Update();           // Process deferred scene loads, etc.
#endif

    // 3. Update Game Logic
    Zenith_Physics::Update(dt);        // Physics simulation (60 Hz fixed timestep)
    Zenith_Scene::Update(dt);          // Entity scripts and component updates
    Flux_Graphics::UploadFrameConstants();  // Camera matrices, etc.

    // 4. Submit Render Tasks
    SubmitRenderTasks();               // Queue rendering work for worker threads
    
    // 5. Wait for Completion
    WaitForRenderTasks();              // Sync with rendering threads
    Zenith_Scene::WaitForUpdateComplete();  // Sync with scene update tasks

    // 6. End Frame
    Flux_MemoryManager::EndFrame();    // Cleanup frame allocations
    Zenith_MemoryManagement::EndFrame();
    Flux_PlatformAPI::EndFrame();
    Flux_Swapchain::EndFrame();        // Present to screen
}
```

**Execution Model:**
- Game logic runs on main thread
- Rendering runs on worker threads (8 Flux workers)
- Physics uses separate Jolt thread pool (`hardware_concurrency - 1` threads)

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

The engine uses a fixed thread pool for rendering and other parallel tasks:

```cpp
// Flux rendering workers (from Zenith_Vulkan.cpp)
constexpr u_int FLUX_NUM_WORKER_THREADS = 8;

// Physics uses Jolt's thread pool (from Zenith_Physics.cpp)
// std::thread::hardware_concurrency() - 1
```

**Thread Roles:**
1. **Main Thread:** Game logic, ECS updates, input handling
2. **Flux Worker Threads 0-7:** Command buffer recording, task execution  
3. **Jolt Physics Thread Pool:** Physics simulation (separate from Flux workers, uses hardware_concurrency - 1 threads)

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

The engine pipelines up to `MAX_FRAMES_IN_FLIGHT` (currently 2) frames:
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
├── AssetHandling/      # Asset loading and management
├── Collections/        # Custom containers (Vector, Pool, etc.)
├── Core/              # Core utilities and engine loop
│   └── Memory/        # Memory allocators
├── DataStream/        # Binary serialization
├── DebugVariables/    # Runtime-tweakable debug variables
├── EntityComponent/   # ECS implementation
│   └── Components/    # Component definitions
├── Editor/            # Editor tools (ZENITH_TOOLS only)
├── Flux/              # Vulkan renderer
│   ├── StaticMeshes/  # Opaque geometry rendering
│   ├── Shadows/       # Shadow map generation
│   ├── DeferredShading/ # Lighting pass
│   ├── Particles/     # Particle systems
│   ├── Terrain/       # Terrain rendering & culling
│   ├── Gizmos/        # 3D transform gizmos (ZENITH_TOOLS only)
│   └── Shaders/       # GLSL shader source
├── Input/             # Keyboard/mouse/gamepad input
├── Maths/             # Math library (GLM wrapper)
├── Physics/           # Jolt Physics integration
├── Profiling/         # CPU profiler
├── StateMachine/      # Game state management
├── TaskSystem/        # Multi-threading infrastructure
├── Vulkan/            # Vulkan platform backend
└── Windows/           # Windows platform layer

Games/
└── Test/              # Test game project
    ├── Assets/       # Game-specific assets
    └── Source/       # Game code

Tools/
├── FluxCompiler/      # Shader compilation tool
└── AssetConverters/   # Mesh/texture converters
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
- GLM - Math library (vector/matrix types via `using` declarations in Zenith_Maths.h)
- Jolt Physics - Physics simulation (`JPH::PhysicsSystem`, `JPH::Body`, etc.)
- GLFW - Window/input management
- Vulkan SDK 1.3 - Graphics API
- ImGui - Debug UI (docking branch for editor)

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
- Terrain LOD streaming (implemented - see [Flux/Terrain/CLAUDE.md](Zenith/Flux/Terrain/CLAUDE.md))

## Debug Features

**Tools Build (`ZENITH_TOOLS` defined):**
- ImGui editor interface
- Visual profiler
- Entity inspector
- **3D Gizmos for entity manipulation** (see below)

**Release Build:**
- Minimal overhead
- No debug variables
- Optimized shaders

## Editor System (Zenith_Editor)

**Location:** `Zenith/Editor/`

The editor provides an ImGui-based interface for scene editing, entity manipulation, and debugging.

**Architecture:**
- **Singleton Pattern:** `Zenith_Editor` provides static methods for global editor state
- **EntityID-Based Selection:** Uses `Zenith_EntityID` instead of raw pointers to prevent dangling references
- **Deferred Operations:** Scene loads and entity deletions are deferred to prevent thread conflicts

**Key Components:**

1. **Menu Bar** (`RenderMenuBar`):
   - File operations (New Scene, Open Scene, Save Scene)
   - Scene state controls (Play, Pause, Stop)
   - Deferred scene loading to prevent concurrent access during rendering

2. **Scene Hierarchy Panel** (`RenderSceneHierarchy`):
   - Lists all entities in the current scene
   - Click to select entities
   - Returns `EntityID` instead of raw pointer for safety

3. **Properties Panel** (`RenderPropertiesPanel`):
   - Displays and edits components of selected entity
   - **Transform editing:** Position, rotation (Euler angles), scale
   - **Entity naming:** Editable entity name field
   - Component-specific editors

4. **Viewport Panel** (`RenderViewport`):
   - Displays game view with ImGui::Image
   - Tracks viewport position and size for mouse coordinate conversion
   - Handles gizmo rendering and interaction
   - Sets `s_bViewportHovered` flag for input filtering

5. **3D Gizmo System** (Flux_Gizmos):
   - **Location:** `Zenith/Flux/Gizmos/`
   - Interactive 3D widgets for manipulating entity transforms
   - Three modes: Translate, Rotate, Scale
   - Visual feedback with color-coded axes (X=Red, Y=Green, Z=Blue)

**Editor Update Timing:**
`Zenith_Editor::Update()` is called at the start of each frame (before rendering) to:
- Process deferred scene loads
- Handle deferred entity deletions
- Process play/stop state transitions (December 2024 fix)
- Update editor state

See "CRITICAL: Editor Update Timing" section above for threading details.

**Play/Stop Mode (December 2024):**
- **Play Button:** Saves current scene to backup DataStream, then transitions to playing state
- **Stop Button:** Restores scene from backup DataStream
- **CRITICAL:** All components must support serialization for play/stop to work correctly
- See [Editor/CLAUDE.md - Play/Stop Bug History](Zenith/Editor/CLAUDE.md) for known issues and fixes

### 3D Gizmo System (Flux_Gizmos)

**Recent Implementation (December 2024):**

The gizmo system allows users to manipulate entity transforms by clicking and dragging 3D widgets in the viewport.

**Files:**
- `Zenith/Flux/Gizmos/Flux_Gizmos.h/cpp` - Core gizmo rendering and interaction
- `Zenith/Gizmos/Zenith_Gizmo.h/cpp` - Utility functions (ray casting, intersection tests)

**Key Features:**

1. **Rendering:**
   - Separate render task submitted each frame
   - Auto-scaling based on camera distance for consistent screen size
   - Color-coded axes: X (Red), Y (Green), Z (Blue)
   - Geometry generated at initialization (arrows, circles, cubes)

2. **Interaction:**
   - Ray-cylinder intersection for arrow picking
   - Extended interaction bounds (10× visual length) for easier clicking from any angle
   - Three-phase interaction: BeginInteraction → UpdateInteraction → EndInteraction
   - Transform manipulation in BeginInteraction stores initial state

3. **Transform Application:**
   - **Translate:** Projects ray onto constraint axis, calculates offset from initial position
   - **Rotate:** Calculates rotation angle around axis based on ray-plane intersection
   - **Scale:** Applies uniform or per-axis scaling based on drag distance

**Critical Implementation Details:**

1. **Coordinate System:**
   - Uses **Vulkan depth range [0, 1]** (not OpenGL's [-1, 1])
   - `ScreenToWorldRay` sets `rayClip.z = 0.0f` for near plane
   - Proper perspective divide: `rayClip / rayClip.w` before transforming to world space

2. **Gizmo Scale:**
   - Calculated in `Render()`: `s_fGizmoScale = distance / GIZMO_AUTO_SCALE_DISTANCE`
   - Ray transformed to local space by dividing by scale: `localRay = (ray - gizmoPos) / scale`
   - Intersection tests performed in local space (unit-sized arrows)

3. **Interaction Bounds:**
   - Visual arrow length: `GIZMO_ARROW_LENGTH = 1.2` (local space)
   - Interaction length: `1.2 × GIZMO_INTERACTION_LENGTH_MULTIPLIER = 1.2 × 10.0 = 12.0`
   - Extended bounds necessary because camera angle can cause ray to hit cylinder far from arrow tip

4. **Input Handling:**
   - Mouse button input uses `glfwGetMouseButton()` (not `glfwGetKey()`)
   - Independent `if` statements (not `if-else`) allow same-frame Begin + Update
   - `WasKeyPressedThisFrame` only true for one frame; `IsKeyDown` true while held

5. **Semaphore Management:**
   - Gizmo render task uses semaphore with max count = 1
   - `WaitForRenderTask()` called every frame in `WaitForRenderTasks()` to prevent overflow
   - Task always submitted once per frame even if not rendering

**Common Issues Fixed:**

1. **Buffer Alignment:** Vulkan staging buffer offsets must be 8-byte aligned for BC1 compressed textures
2. **Raycast Calculation:** Must use Vulkan depth range [0, 1], not OpenGL [-1, 1]
3. **Mouse Input:** Mouse buttons require `glfwGetMouseButton()`, not `glfwGetKey()`
4. **Interaction Bounds:** Visual arrow too short for clicking; extended to 10× for usability
5. **UpdateInteraction Not Called:** Fixed by removing `else` keywords between Begin/Update/End checks

**Debug Logging:**
Use `EDITOR_LOG()` macro for gizmo-specific logging (only active in ZENITH_TOOLS builds).

**Known Limitations:**
- Transform manipulation math may need refinement for accurate final positions
- Currently no visual feedback for active axis during drag
- No snapping or grid alignment (planned feature)

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
